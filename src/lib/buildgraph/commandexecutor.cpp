/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/
#include "commandexecutor.h"
#include "command.h"
#include "buildgraph.h"
#include "processoutput.h"

#include <buildgraph/artifact.h>
#include <buildgraph/transformer.h>
#include <tools/logger.h>
#include <tools/platformglobals.h>
#include <jsextensions/file.h>
#include <jsextensions/textfile.h>
#include <jsextensions/process.h>

#include <QtConcurrentRun>
#include <QDebug>
#include <QDir>
#include <QFutureWatcher>
#include <QProcess>
#include <QScriptEngine>
#include <QThread>
#include <QTemporaryFile>

namespace qbs {

struct JavaScriptCommandFutureResult
{
    bool success;
    QString errorMessage;
    CodeLocation errorLocation;
};

class JavaScriptCommandFutureWatcher : public QFutureWatcher<JavaScriptCommandFutureResult>
{
public:
    JavaScriptCommandFutureWatcher(QObject *parent)
        : QFutureWatcher<JavaScriptCommandFutureResult>(parent)
    {}
};

static QStringList populateExecutableSuffixes()
{
    QStringList result;
    result << QString();
#ifdef Q_OS_WIN
    result << QLatin1String(".com") << QLatin1String(".exe")
           << QLatin1String(".bat") << QLatin1String(".cmd");
#endif
    return result;
}

const QStringList CommandExecutor::m_executableSuffixes = populateExecutableSuffixes();

CommandExecutor::CommandExecutor(QObject *parent)
    : QObject(parent)
    , m_transformer(0)
    , m_processCommand(0)
    , m_process(0)
    , m_mainThreadScriptEngine(0)
    , m_jsCommand(0)
    , m_jsFutureWatcher(0)
{
    connect(&m_process, SIGNAL(error(QProcess::ProcessError)), this, SLOT(onProcessError(QProcess::ProcessError)));
    connect(&m_process, SIGNAL(finished(int)), this, SLOT(onProcessFinished(int)));
}

void CommandExecutor::setProcessEnvironment(const QProcessEnvironment &processEnvironment)
{
    m_process.setProcessEnvironment(processEnvironment);
}

void CommandExecutor::waitForFinished()
{
    m_process.waitForFinished(-1);
    if (m_jsFutureWatcher && m_jsFutureWatcher->isRunning())
        m_jsFutureWatcher->waitForFinished();
}

void CommandExecutor::start(Transformer *transformer, AbstractCommand *cmd)
{
    m_transformer = transformer;
    m_processCommand = 0;
    m_jsCommand = 0;

    switch (cmd->type()) {
    case AbstractCommand::AbstractCommandType:
        qWarning("CommandExecutor can't execute abstract commands.");
        return;
    case AbstractCommand::ProcessCommandType:
        m_processCommand = static_cast<ProcessCommand*>(cmd);
        startProcessCommand();
        return;
    case AbstractCommand::JavaScriptCommandType:
        m_jsCommand = static_cast<JavaScriptCommand*>(cmd);
        startJavaScriptCommand();
        return;
    }

    emit error("CommandExecutor: unknown command type.");
    return;
}

static QHash<QString, TextColor> setupColorTable()
{
    QHash<QString, TextColor> colorTable;
    colorTable["compiler"] = TextColorDefault;
    colorTable["linker"] = TextColorDarkGreen;
    colorTable["codegen"] = TextColorDarkYellow;
    colorTable["filegen"] = TextColorDarkYellow;
    return colorTable;
}

void CommandExecutor::printCommandInfo(AbstractCommand *cmd)
{
    if (!cmd->description().isEmpty()) {
        static QHash<QString, TextColor> colorTable = setupColorTable();
        qbsInfo() << DontPrintLogLevel << LogOutputStdOut
                  << colorTable.value(cmd->highlight(), TextColorDefault)
                  << cmd->description();
    }
}

static QString commandArgsToString(const QStringList &args)
{
    QString result;
    QRegExp ws("\\s");
    bool first = true;
    foreach (const QString &arg, args) {
        if (first)
            first = false;
        else
            result += QLatin1Char(' ');

        if (arg.contains(ws))
            result += QLatin1Char('"') + arg + QLatin1Char('"');
        else
            result += arg;
    }
    return result;
}

void CommandExecutor::startProcessCommand()
{
    Q_ASSERT(m_process.state() == QProcess::NotRunning);

    printCommandInfo(m_processCommand);
    if (!m_processCommand->isSilent()) {
        QString commandLine = m_processCommand->program() + QLatin1Char(' ') + commandArgsToString(m_processCommand->arguments());
        qbsInfo() << DontPrintLogLevel << LogOutputStdOut << commandLine;
    }
    if (qbsLogLevel(LoggerDebug)) {
        qbsDebug() << "[EXEC] " << m_processCommand->program() + QLatin1Char(' ') + commandArgsToString(m_processCommand->arguments());
    }

    // Automatically use response files, if the command line gets to long.
    QStringList arguments = m_processCommand->arguments();
    if (!m_processCommand->responseFileUsagePrefix().isEmpty()) {
        int commandLineLength = m_processCommand->program().length() + 1;
        for (int i = m_processCommand->arguments().count(); --i >= 0;)
            commandLineLength += m_processCommand->arguments().at(i).length();
        if (m_processCommand->responseFileThreshold() >= 0 && commandLineLength > m_processCommand->responseFileThreshold()) {
            if (qbsLogLevel(LoggerDebug))
                qbsDebug("[EXEC] Using response file. Threshold is %d. Command line length %d.", m_processCommand->responseFileThreshold(), commandLineLength);

            // The QTemporaryFile keeps a handle on the file, even if closed.
            // On Windows, some commands (e.g. msvc link.exe) won't accept that.
            // We need to delete the file manually, later.
            QTemporaryFile responseFile;
            responseFile.setAutoRemove(false);
            responseFile.setFileTemplate(QDir::tempPath() + "/qbsresp");
            if (!responseFile.open()) {
                QString errorMessage = "Cannot create response file.";
                emit error(errorMessage);
                return;
            }
            for (int i = 0; i < m_processCommand->arguments().count(); ++i) {
                responseFile.write(m_processCommand->arguments().at(i).toLocal8Bit());
                responseFile.write("\n");
            }
            responseFile.close();
            m_responseFileName = responseFile.fileName();
            arguments.clear();
            arguments += QDir::toNativeSeparators(m_processCommand->responseFileUsagePrefix() + responseFile.fileName());
            if (qbsLogLevel(LoggerDebug))
                qbsDebug("[EXEC] command line with response file: %s %s", qPrintable(m_processCommand->program()), qPrintable(commandArgsToString(arguments)));
        }
    }

    QString program;
    if (FileInfo::isAbsolute(m_processCommand->program())) {
#ifdef Q_OS_WIN
        program = findProcessCommandBySuffix();
#else
        program = m_processCommand->program();
#endif
    } else {
        program = findProcessCommandInPath();
    }

    m_process.setWorkingDirectory(m_processCommand->workingDir());
    m_process.start(program, arguments);
}

QByteArray CommandExecutor::filterProcessOutput(const QByteArray &output, const QString &filterFunctionSource)
{
    if (filterFunctionSource.isEmpty())
        return output;

    QScriptValue filterFunction = m_mainThreadScriptEngine->evaluate("var f = " + filterFunctionSource + "; f");
    if (!filterFunction.isFunction()) {
        emit error(QString("Error in filter function: %1.\n%2").arg(filterFunctionSource, filterFunction.toString()));
        return output;
    }

    QScriptValue outputArg = m_mainThreadScriptEngine->newArray(1);
    outputArg.setProperty(0, m_mainThreadScriptEngine->toScriptValue(QString::fromLatin1(output)));
    QScriptValue filteredOutput = filterFunction.call(m_mainThreadScriptEngine->undefinedValue(), outputArg);
    if (filteredOutput.isError()) {
        emit error(QString("Error when calling ouput filter function: %1").arg(filteredOutput.toString()));
        return output;
    }

    return filteredOutput.toString().toLocal8Bit();
}

static QStringList filePathsFromInputArtifacts(Transformer *transformer)
{
    QStringList filePathList;

    if (transformer) {
        foreach (Artifact *artifact, transformer->inputs)
            filePathList.append(artifact->filePath());
    }

    return filePathList;
}

void CommandExecutor::sendProcessOutput(bool logCommandLine)
{
    QString commandLine = m_processCommand->program();
    if (!m_processCommand->arguments().isEmpty())
        commandLine += QLatin1Char(' ') + commandArgsToString(m_processCommand->arguments());

    QByteArray processStdOut = filterProcessOutput(m_process.readAllStandardOutput(), m_processCommand->stdoutFilterFunction());
    QByteArray processStdErr = filterProcessOutput(m_process.readAllStandardError(), m_processCommand->stderrFilterFunction());

    bool processOutputEmpty = processStdOut.isEmpty() && processStdErr.isEmpty();
    if (logCommandLine || !processOutputEmpty) {
        qbsInfo() << DontPrintLogLevel << commandLine << (processOutputEmpty ? "" : "\n")
                  << processStdOut << processStdErr;
    }

    ProcessOutput processOutput;
    processOutput.setCommandLine(commandLine);
    processOutput.setStandardOutput(processStdOut);
    processOutput.setStandardError(processStdErr);
    processOutput.setFilePaths(filePathsFromInputArtifacts(m_transformer));
    Logger::instance().sendProcessOutput(processOutput);
}

void CommandExecutor::onProcessError(QProcess::ProcessError processError)
{
    removeResponseFile();
    sendProcessOutput(true);
    QString errorMessage;
    const QString binary = QDir::toNativeSeparators(m_processCommand->program());
    switch (processError) {
    case QProcess::FailedToStart:
        errorMessage = QString::fromLatin1("The process '%1' could not be started: %2").
                arg(binary, m_process.errorString());
        break;
    case QProcess::Crashed:
        errorMessage = QString::fromLatin1("The process '%1' crashed.").arg(binary);
        break;
    case QProcess::Timedout:
        errorMessage = QString::fromLatin1("The process '%1' timed out.").arg(binary);
        break;
    case QProcess::ReadError:
        errorMessage = QString::fromLatin1("Error reading process output from '%1'.").arg(binary);
        break;
    case QProcess::WriteError:
        errorMessage = QString::fromLatin1("Error writing to process '%1'.").arg(binary);
        break;
    default:
        errorMessage = QString::fromLatin1("Unknown process error running '%1'.").arg(binary);
        break;
    }
    emit error(errorMessage);
}

void CommandExecutor::onProcessFinished(int exitCode)
{
    removeResponseFile();
    bool errorOccurred = exitCode > m_processCommand->maxExitCode();
    sendProcessOutput(errorOccurred);
    if (errorOccurred) {
        QString msg = "Process failed with exit code %1.";
        emit error(msg.arg(exitCode));
        return;
    }

    emit finished();
}

class JSRunner
{
public:
    typedef JavaScriptCommandFutureResult result_type;

    JSRunner(JavaScriptCommand *jsCommand)
        : m_jsCommand(jsCommand)
    {}

    JavaScriptCommandFutureResult operator() (Transformer *transformer)
    {
        result_type result;
        result.success = true;
        QThread *currentThread = QThread::currentThread();
        QScriptEngine *scriptEngine = m_scriptEnginesPerThread.value(currentThread);
        if (!scriptEngine) {
            scriptEngine = new QScriptEngine();
            m_scriptEnginesPerThread.insert(currentThread, scriptEngine);

            QScriptValue extensionObject = scriptEngine->globalObject();
            File::init(extensionObject);
            TextFile::init(extensionObject);
            Process::init(extensionObject);
        }

        QString trafoPtrStr = QString::number((qulonglong)transformer);
        if (scriptEngine->globalObject().property("_qbs_transformer_ptr").toString() != trafoPtrStr) {
            scriptEngine->globalObject().setProperty("_qbs_transformer_ptr", scriptEngine->toScriptValue(trafoPtrStr));

            Artifact *someOutputArtifact = *transformer->outputs.begin();
            if (someOutputArtifact->product) {
                ResolvedProduct::Ptr product = someOutputArtifact->product->rProduct;
                BuildGraph::setupScriptEngineForProduct(scriptEngine, product, transformer->rule);
            }
            transformer->setupInputs(scriptEngine, scriptEngine->globalObject());
            transformer->setupOutputs(scriptEngine, scriptEngine->globalObject());
        }

        scriptEngine->pushContext();
        for (QVariantMap::const_iterator it = m_jsCommand->properties().constBegin(); it != m_jsCommand->properties().constEnd(); ++it)
            scriptEngine->currentContext()->activationObject().setProperty(it.key(), scriptEngine->toScriptValue(it.value()));

        scriptEngine->evaluate(m_jsCommand->sourceCode());
        if (scriptEngine->hasUncaughtException()) {
            result.success = false;
            result.errorMessage = scriptEngine->uncaughtException().toString();
            result.errorLocation = m_jsCommand->codeLocation();
            result.errorLocation.line += scriptEngine->uncaughtExceptionLineNumber();
        }
        scriptEngine->popContext();
        scriptEngine->clearExceptions();
        return result;
    }

private:
    static QHash<QThread *, QScriptEngine *> m_scriptEnginesPerThread;
    JavaScriptCommand *m_jsCommand;
};

QHash<QThread *, QScriptEngine *> JSRunner::m_scriptEnginesPerThread;

void CommandExecutor::startJavaScriptCommand()
{
    printCommandInfo(m_jsCommand);
    QFuture<JSRunner::result_type> future = QtConcurrent::run(JSRunner(m_jsCommand), m_transformer);
    if (!m_jsFutureWatcher) {
        m_jsFutureWatcher = new JavaScriptCommandFutureWatcher(this);
        connect(m_jsFutureWatcher, SIGNAL(finished()), SLOT(onJavaScriptCommandFinished()));
    }
    m_jsFutureWatcher->setFuture(future);
}

void CommandExecutor::onJavaScriptCommandFinished()
{
    JavaScriptCommandFutureResult result = m_jsFutureWatcher->future().result();
    if (result.success) {
        emit finished();
    } else {
        qbsDebug() << DontPrintLogLevel << "JS context:\n" << m_jsCommand->properties();
        qbsDebug() << DontPrintLogLevel << "JS code:\n" << m_jsCommand->sourceCode();
        QString msg = "Error while executing JavaScriptCommand (%1:%2):\n";
        msg += result.errorMessage;
        emit error(msg.arg(QDir::toNativeSeparators(result.errorLocation.fileName)).arg(result.errorLocation.line));
    }
}

void CommandExecutor::removeResponseFile()
{
    if (m_responseFileName.isEmpty())
        return;
    QFile::remove(m_responseFileName);
    m_responseFileName.clear();
}

QString CommandExecutor::findProcessCommandInPath()
{
    Artifact *outputNode = (*m_transformer->outputs.begin());
    ResolvedProduct *product = outputNode->product->rProduct.data();
    QString fullProgramPath = product->executablePathCache.value(m_processCommand->program());
    if (!fullProgramPath.isEmpty())
        return fullProgramPath;

    fullProgramPath = m_processCommand->program();
    if (qbsLogLevel(LoggerTrace))
        qbsTrace() << "[EXEC] looking for executable in PATH " << fullProgramPath;
    const QProcessEnvironment &buildEnvironment = product->buildEnvironment;
    QStringList pathEnv = buildEnvironment.value("PATH").split(QLatin1Char(nativePathVariableSeparator), QString::SkipEmptyParts);
#ifdef Q_OS_WIN
    pathEnv.prepend(QLatin1String("."));
#endif
    for (int i = 0; i < pathEnv.count(); ++i) {
        QString directory = pathEnv.at(i);
        if (directory == QLatin1String("."))
            directory = m_processCommand->workingDir();
        if (!directory.isEmpty()) {
            const QChar lastChar = directory.at(directory.count() - 1);
            if (lastChar != QLatin1Char('/') && lastChar != QLatin1Char('\\'))
                directory.append(QLatin1Char('/'));
        }
        if (findProcessCandidateCheck(directory, fullProgramPath, fullProgramPath))
            break;
    }
    product->executablePathCache.insert(m_processCommand->program(), fullProgramPath);
    return fullProgramPath;
}

QString CommandExecutor::findProcessCommandBySuffix()
{
    Artifact *outputNode = (*m_transformer->outputs.begin());
    ResolvedProduct *product = outputNode->product->rProduct.data();
    QString fullProgramPath = product->executablePathCache.value(m_processCommand->program());
    if (!fullProgramPath.isEmpty())
        return fullProgramPath;

    fullProgramPath = m_processCommand->program();
    if (qbsLogLevel(LoggerTrace))
        qbsTrace() << "[EXEC] looking for executable by suffix " << fullProgramPath;
    const QString emptyDirectory;
    findProcessCandidateCheck(emptyDirectory, fullProgramPath, fullProgramPath);
    product->executablePathCache.insert(m_processCommand->program(), fullProgramPath);
    return fullProgramPath;
}

bool CommandExecutor::findProcessCandidateCheck(const QString &directory, const QString &program, QString &fullProgramPath)
{
    for (int i = 0; i < m_executableSuffixes.count(); ++i) {
        QString candidate = directory + program + m_executableSuffixes.at(i);
        if (qbsLogLevel(LoggerTrace))
            qbsTrace() << "[EXEC] candidate: " << candidate;
        if (FileInfo::exists(candidate)) {
            fullProgramPath = candidate;
            return true;
        }
    }
    return false;
}

} // namespace qbs
