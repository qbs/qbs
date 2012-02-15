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

#include <QtConcurrentRun>
#include <QDebug>
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
};

class JavaScriptCommandFutureWatcher : public QFutureWatcher<JavaScriptCommandFutureResult>
{
public:
    JavaScriptCommandFutureWatcher(QObject *parent)
        : QFutureWatcher<JavaScriptCommandFutureResult>(parent)
    {}
};

CommandExecutor::CommandExecutor(QObject *parent)
    : QObject(parent)
    , m_processCommand(0)
    , m_process(0)
    , m_mainThreadScriptEngine(0)
    , m_transformer(0)
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
    if (m_process.state() == QProcess::Running)
        m_process.waitForFinished(-1);
    if (m_jsFutureWatcher && m_jsFutureWatcher->isRunning())
        m_jsFutureWatcher->waitForFinished();
}

void CommandExecutor::start(Transformer *transformer, AbstractCommand *cmd)
{
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
        m_transformer = transformer;
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

void CommandExecutor::startProcessCommand()
{
    Q_ASSERT(m_process.state() == QProcess::NotRunning);

    printCommandInfo(m_processCommand);
    if (!m_processCommand->isSilent()) {
        QString commandLine = m_processCommand->program() + QLatin1Char(' ') + m_processCommand->arguments().join(" ");
        qbsInfo() << DontPrintLogLevel << LogOutputStdOut << commandLine;
    }
    if (qbsLogLevel(LoggerDebug)) {
        qbsDebug() << "[EXEC] " << m_processCommand->program() + QLatin1Char(' ') + m_processCommand->arguments().join(" ");
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
            arguments.clear();
            arguments += QDir::toNativeSeparators(m_processCommand->responseFileUsagePrefix() + responseFile.fileName());
            if (qbsLogLevel(LoggerDebug))
                qbsDebug("[EXEC] command line with response file: %s %s", qPrintable(m_processCommand->program()), qPrintable(arguments.join(" ")));
        }
    }

    m_process.setWorkingDirectory(m_processCommand->workingDir());
    m_process.start(m_processCommand->program(), arguments);
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

void CommandExecutor::sendProcessOutput(bool logCommandLine)
{
    QString commandLine = m_processCommand->program();
    if (!m_processCommand->arguments().isEmpty()) {
        commandLine += ' ';
        commandLine += m_processCommand->arguments().join(" ");
    }

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
    Logger::instance().sendProcessOutput(processOutput);
}

void CommandExecutor::onProcessError(QProcess::ProcessError processError)
{
    removeResponseFile();
    sendProcessOutput(true);
    QString errorMessage;
    switch (processError) {
    case QProcess::FailedToStart:
        errorMessage = "Process could not be started.";
        break;
    case QProcess::Crashed:
        errorMessage = "Process crashed.";
        break;
    case QProcess::Timedout:
        errorMessage = "Process timed out.";
        break;
    case QProcess::ReadError:
        errorMessage = "Error when reading process output.";
        break;
    case QProcess::WriteError:
        errorMessage = "Error when writing to process.";
        break;
    default:
        errorMessage = "Unknown process error.";
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

            // import script extension plugins
            foreach (const QString &name, scriptEngine->availableExtensions()) {
                if (!name.startsWith("qbs"))
                    continue;
                QScriptValue e = scriptEngine->importExtension(name);
                if (e.isError()) {
                    qbsWarning("JS thread %x, unable to load %s into QScriptEngine: %s",
                               (void*)currentThread,
                               qPrintable(name),
                               qPrintable(e.toString()));
                }
                qbsDebug("JS thread %x, script plugin loaded: %s", (void*)currentThread, qPrintable(name));
            }
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
        }
        scriptEngine->popContext();
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
        qbsInfo() << DontPrintLogLevel << "JS context:\n" << m_jsCommand->properties();
        qbsInfo() << DontPrintLogLevel << "JS code:\n" << m_jsCommand->sourceCode();
        QString msg = "Error while executing JavaScriptCommand: ";
        msg += result.errorMessage;
        emit error(msg);
    }
}

void CommandExecutor::removeResponseFile()
{
    if (m_responseFileName.isEmpty())
        return;
    QFile::remove(m_responseFileName);
    m_responseFileName.clear();
}

} // namespace qbs
