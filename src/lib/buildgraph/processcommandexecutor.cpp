/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "processcommandexecutor.h"

#include "artifact.h"
#include "buildgraph.h"
#include "command.h"
#include "transformer.h"

#include <language/language.h>
#include <logging/logger.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>

#include <QDir>
#include <QScriptEngine>
#include <QScriptValue>
#include <QTemporaryFile>

namespace qbs {
namespace Internal {

static QStringList populateExecutableSuffixes()
{
    QStringList result;
    result << QString();
    if (HostOsInfo::isWindowsHost()) {
        result << QLatin1String(".com") << QLatin1String(".exe")
               << QLatin1String(".bat") << QLatin1String(".cmd");
    }
    return result;
}

const QStringList ProcessCommandExecutor::m_executableSuffixes = populateExecutableSuffixes();

ProcessCommandExecutor::ProcessCommandExecutor(QObject *parent) : AbstractCommandExecutor(parent)
{
    connect(&m_process, SIGNAL(error(QProcess::ProcessError)),  SLOT(onProcessError()));
    connect(&m_process, SIGNAL(finished(int)), SLOT(onProcessFinished(int)));
}

void ProcessCommandExecutor::setProcessEnvironment(const QProcessEnvironment &processEnvironment)
{
    m_process.setProcessEnvironment(processEnvironment);
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

void ProcessCommandExecutor::doStart()
{
    Q_ASSERT(m_process.state() == QProcess::NotRunning);

    const ProcessCommand * const cmd = processCommand();
    m_commandLine = cmd->program();
    QStringList arguments = cmd->arguments();
    if (!arguments.isEmpty())
        m_commandLine.append(QLatin1Char(' ')).append(commandArgsToString(arguments));
    if (!cmd->isSilent())
        qbsInfo() << DontPrintLogLevel << LogOutputStdOut << m_commandLine;
    qbsDebug() << "[EXEC] " << m_commandLine;
    if (dryRun()) {
        emit finished();
        return;
    }

    // Automatically use response files, if the command line gets to long.
    if (!cmd->responseFileUsagePrefix().isEmpty()) {
        int commandLineLength = cmd->program().length() + 1;
        for (int i = cmd->arguments().count(); --i >= 0;)
            commandLineLength += cmd->arguments().at(i).length();
        if (cmd->responseFileThreshold() >= 0 && commandLineLength > cmd->responseFileThreshold()) {
            if (qbsLogLevel(LoggerDebug)) {
                qbsDebug("[EXEC] Using response file. Threshold is %d. Command line length %d.",
                        cmd->responseFileThreshold(), commandLineLength);
            }

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
            for (int i = 0; i < cmd->arguments().count(); ++i) {
                responseFile.write(cmd->arguments().at(i).toLocal8Bit());
                responseFile.write("\n");
            }
            responseFile.close();
            m_responseFileName = responseFile.fileName();
            arguments.clear();
            arguments += QDir::toNativeSeparators(cmd->responseFileUsagePrefix()
                    + responseFile.fileName());
            qbsDebug("[EXEC] command line with response file: %s %s",
                     qPrintable(cmd->program()),
                     qPrintable(commandArgsToString(arguments)));
        }
    }

    QString program;
    if (FileInfo::isAbsolute(cmd->program()))
        program = HostOsInfo::isWindowsHost() ? findProcessCommandBySuffix() : cmd->program();
    else
        program = findProcessCommandInPath();

    m_process.setWorkingDirectory(cmd->workingDir());
    m_process.start(program, arguments);
}

void ProcessCommandExecutor::waitForFinished()
{
    m_process.waitForFinished(-1);
}

QString ProcessCommandExecutor::filterProcessOutput(const QByteArray &_output,
        const QString &filterFunctionSource)
{
    const QString output = QString::fromLocal8Bit(_output);
    if (filterFunctionSource.isEmpty())
        return output;

    QScriptValue filterFunction = scriptEngine()->evaluate("var f = " + filterFunctionSource + "; f");
    if (!filterFunction.isFunction()) {
        emit error(QString("Error in filter function: %1.\n%2")
                .arg(filterFunctionSource, filterFunction.toString()));
        return output;
    }

    QScriptValue outputArg = scriptEngine()->newArray(1);
    outputArg.setProperty(0, scriptEngine()->toScriptValue(output));
    QScriptValue filteredOutput = filterFunction.call(scriptEngine()->undefinedValue(), outputArg);
    if (filteredOutput.isError()) {
        emit error(QString("Error when calling ouput filter function: %1")
                .arg(filteredOutput.toString()));
        return output;
    }

    return filteredOutput.toString();
}

void ProcessCommandExecutor::sendProcessOutput(bool dueToError)
{
    const QString processStdOut = filterProcessOutput(m_process.readAllStandardOutput(),
            processCommand()->stdoutFilterFunction());
    const QString processStdErr = filterProcessOutput(m_process.readAllStandardError(),
            processCommand()->stderrFilterFunction());
    const bool processOutputEmpty = processStdOut.isEmpty() && processStdErr.isEmpty();
    if (processOutputEmpty && !dueToError)
        return;
    (dueToError ? qbsError() : qbsInfo()) << DontPrintLogLevel << m_commandLine
            << (processOutputEmpty ? "" : "\n") << processStdOut << processStdErr;
}

void ProcessCommandExecutor::onProcessError()
{
    removeResponseFile();
    sendProcessOutput(true);
    QString errorMessage;
    const QString binary = QDir::toNativeSeparators(processCommand()->program());
    switch (m_process.error()) {
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

void ProcessCommandExecutor::onProcessFinished(int exitCode)
{
    removeResponseFile();
    const bool errorOccurred = exitCode > processCommand()->maxExitCode();
    sendProcessOutput(errorOccurred);
    if (errorOccurred) {
        QString msg = "Process failed with exit code %1.";
        emit error(msg.arg(exitCode));
        return;
    }

    emit finished();
}

void ProcessCommandExecutor::removeResponseFile()
{
    if (m_responseFileName.isEmpty())
        return;
    QFile::remove(m_responseFileName);
    m_responseFileName.clear();
}

QString ProcessCommandExecutor::findProcessCommandInPath()
{
    Artifact * const outputNode = *transformer()->outputs.begin();
    ResolvedProduct * const product = outputNode->product->rProduct.data();
    const ProcessCommand * const cmd = processCommand();
    QString fullProgramPath = product->executablePathCache.value(cmd->program());
    if (!fullProgramPath.isEmpty())
        return fullProgramPath;

    fullProgramPath = cmd->program();
    if (qbsLogLevel(LoggerTrace))
        qbsTrace() << "[EXEC] looking for executable in PATH " << fullProgramPath;
    const QProcessEnvironment &buildEnvironment = product->buildEnvironment;
    QStringList pathEnv = buildEnvironment.value("PATH").split(HostOsInfo::pathListSeparator(),
            QString::SkipEmptyParts);
    if (HostOsInfo::isWindowsHost())
        pathEnv.prepend(QLatin1String("."));
    for (int i = 0; i < pathEnv.count(); ++i) {
        QString directory = pathEnv.at(i);
        if (directory == QLatin1String("."))
            directory = cmd->workingDir();
        if (!directory.isEmpty()) {
            const QChar lastChar = directory.at(directory.count() - 1);
            if (lastChar != QLatin1Char('/') && lastChar != QLatin1Char('\\'))
                directory.append(QLatin1Char('/'));
        }
        if (findProcessCandidateCheck(directory, fullProgramPath, fullProgramPath))
            break;
    }
    product->executablePathCache.insert(cmd->program(), fullProgramPath);
    return fullProgramPath;
}

QString ProcessCommandExecutor::findProcessCommandBySuffix()
{
    Artifact * const outputNode = *transformer()->outputs.begin();
    ResolvedProduct * const product = outputNode->product->rProduct.data();
    const ProcessCommand * const cmd = processCommand();
    QString fullProgramPath = product->executablePathCache.value(cmd->program());
    if (!fullProgramPath.isEmpty())
        return fullProgramPath;

    fullProgramPath = cmd->program();
    if (qbsLogLevel(LoggerTrace))
        qbsTrace() << "[EXEC] looking for executable by suffix " << fullProgramPath;
    const QString emptyDirectory;
    findProcessCandidateCheck(emptyDirectory, fullProgramPath, fullProgramPath);
    product->executablePathCache.insert(cmd->program(), fullProgramPath);
    return fullProgramPath;
}

bool ProcessCommandExecutor::findProcessCandidateCheck(const QString &directory,
        const QString &program, QString &fullProgramPath)
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

const ProcessCommand *ProcessCommandExecutor::processCommand() const
{
    return static_cast<const ProcessCommand *>(command());
}

} // namespace Internal
} // namespace qbs
