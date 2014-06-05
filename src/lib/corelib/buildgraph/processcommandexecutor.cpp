/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
#include "command.h"
#include "transformer.h"

#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <tools/processresult.h>
#include <tools/processresult_p.h>
#include <tools/qbsassert.h>

#include <QDir>
#include <QScriptEngine>
#include <QScriptValue>
#include <QTemporaryFile>
#include <QTimer>

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

ProcessCommandExecutor::ProcessCommandExecutor(const Logger &logger, QObject *parent)
    : AbstractCommandExecutor(logger, parent)
{
    connect(&m_process, SIGNAL(error(QProcess::ProcessError)),  SLOT(onProcessError()));
    connect(&m_process, SIGNAL(finished(int)), SLOT(onProcessFinished(int)));
}

// returns an empty string or one that starts with a space!
static QString commandArgsToString(const QStringList &args)
{
    QString result;
    QRegExp ws(QLatin1String("\\s"));
    foreach (const QString &arg, args) {
        result += QLatin1Char(' ');

        if (arg.contains(ws) || arg.isEmpty())
            result += QLatin1Char('"') + arg + QLatin1Char('"');
        else
            result += arg;
    }
    return result;
}

void ProcessCommandExecutor::doStart()
{
    QBS_ASSERT(m_process.state() == QProcess::NotRunning, return);

    const ProcessCommand * const cmd = processCommand();
    QString program = cmd->program();
    if (FileInfo::isAbsolute(cmd->program())) {
        if (HostOsInfo::isWindowsHost())
            program = findProcessCommandBySuffix();
    } else {
        program = findProcessCommandInPath();
    }

    QProcessEnvironment env = m_buildEnvironment;
    const QProcessEnvironment &additionalVariables = cmd->environment();
    foreach (const QString &key, additionalVariables.keys())
        env.insert(key, additionalVariables.value(key));
    m_process.setProcessEnvironment(env);

    QStringList arguments = cmd->arguments();
    QString argString = commandArgsToString(arguments);

    if (dryRun()) {
        QTimer::singleShot(0, this, SIGNAL(finished())); // Don't call back on the caller.
        return;
    }

    if (!cmd->workingDir().isEmpty()) {
        FileInfo fi(cmd->workingDir());
        if (!fi.exists() || !fi.isDir()) {
            emit finished(ErrorInfo(Tr::tr("The working directory '%1' for process '%2' "
                                           "is invalid.").arg(cmd->workingDir(), program),
                                    cmd->codeLocation()));
            return;
        }
    }

    // Automatically use response files, if the command line gets to long.
    if (!cmd->responseFileUsagePrefix().isEmpty()) {
        const int commandLineLength = program.length() + argString.length();
        if (cmd->responseFileThreshold() >= 0 && commandLineLength > cmd->responseFileThreshold()) {
            if (logger().debugEnabled()) {
                logger().qbsDebug() << QString::fromLocal8Bit("[EXEC] Using response file. "
                        "Threshold is %1. Command line length %2.")
                        .arg(cmd->responseFileThreshold()).arg(commandLineLength);
            }

            // The QTemporaryFile keeps a handle on the file, even if closed.
            // On Windows, some commands (e.g. msvc link.exe) won't accept that.
            // We need to delete the file manually, later.
            QTemporaryFile responseFile;
            responseFile.setAutoRemove(false);
            responseFile.setFileTemplate(QDir::tempPath() + QLatin1String("/qbsresp"));
            if (!responseFile.open()) {
                emit finished(ErrorInfo(Tr::tr("Cannot create response file '%1'.")
                                 .arg(responseFile.fileName())));
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
        }
    }

    logger().qbsDebug() << "[EXEC] Running external process; full command line is: " << program
                        << commandArgsToString(arguments);
    logger().qbsTrace() << "[EXEC] Additional environment:" << additionalVariables.toStringList();
    m_process.setWorkingDirectory(cmd->workingDir());
    m_process.start(program, arguments);

    m_program = program;
    m_arguments = arguments;
}

void ProcessCommandExecutor::cancel()
{
    m_process.terminate();
    if (!m_process.waitForFinished(1000))
        m_process.kill();
}

QString ProcessCommandExecutor::filterProcessOutput(const QByteArray &_output,
        const QString &filterFunctionSource)
{
    const QString output = QString::fromLocal8Bit(_output);
    if (filterFunctionSource.isEmpty())
        return output;

    QScriptValue filterFunction = scriptEngine()->evaluate(QLatin1String("var f = ")
                                                           + filterFunctionSource
                                                           + QLatin1String("; f"));
    if (!filterFunction.isFunction()) {
        logger().printWarning(ErrorInfo(Tr::tr("Error in filter function: %1.\n%2")
                         .arg(filterFunctionSource, filterFunction.toString())));
        return output;
    }

    QScriptValue outputArg = scriptEngine()->newArray(1);
    outputArg.setProperty(0, scriptEngine()->toScriptValue(output));
    QScriptValue filteredOutput = filterFunction.call(scriptEngine()->undefinedValue(), outputArg);
    if (scriptEngine()->hasErrorOrException(filteredOutput)) {
        logger().printWarning(ErrorInfo(Tr::tr("Error when calling output filter function: %1")
                         .arg(filteredOutput.toString())));
        return output;
    }

    return filteredOutput.toString();
}

void ProcessCommandExecutor::sendProcessOutput(bool success)
{
    ProcessResult result;
    result.d->executableFilePath = m_program;
    result.d->arguments = m_arguments;
    result.d->workingDirectory = m_process.workingDirectory();
    if (result.workingDirectory().isEmpty())
        result.d->workingDirectory = QDir::currentPath();
    result.d->exitCode = m_process.exitCode();
    result.d->exitStatus = m_process.exitStatus();
    result.d->success = success;

    QString tmp = filterProcessOutput(m_process.readAllStandardOutput(),
                                      processCommand()->stdoutFilterFunction());
    if (!tmp.isEmpty()) {
        if (tmp.endsWith(QLatin1Char('\n')))
            tmp.chop(1);
        result.d->stdOut = tmp.split(QLatin1Char('\n'));
    }
    tmp = filterProcessOutput(m_process.readAllStandardError(),
                              processCommand()->stderrFilterFunction());
    if (!tmp.isEmpty()) {
        if (tmp.endsWith(QLatin1Char('\n')))
            tmp.chop(1);
        result.d->stdErr = tmp.split(QLatin1Char('\n'));
    }

    emit reportProcessResult(result);
}

void ProcessCommandExecutor::onProcessError()
{
    switch (m_process.error()) {
    case QProcess::FailedToStart: {
        removeResponseFile();
        const QString binary = QDir::toNativeSeparators(processCommand()->program());
        emit finished(ErrorInfo(Tr::tr("The process '%1' could not be started: %2")
                                .arg(binary, m_process.errorString())));
        break;
    }
    case QProcess::Crashed:
        break; // Ignore. Will be handled by onProcessFinished().
    default:
        logger().qbsWarning() << "QProcess error: " << m_process.errorString();
    }
}

void ProcessCommandExecutor::onProcessFinished(int exitCode)
{
    removeResponseFile();
    const bool crashed = m_process.exitStatus() == QProcess::CrashExit;
    const bool errorOccurred = crashed
            || quint32(exitCode) > quint32(processCommand()->maxExitCode());
    sendProcessOutput(!errorOccurred);

    if (Q_UNLIKELY(crashed))
        emit finished(ErrorInfo(Tr::tr("Process crashed.")));
    else if (Q_UNLIKELY(errorOccurred))
        emit finished(ErrorInfo(Tr::tr("Process failed with exit code %1.").arg(exitCode)));
    else
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
    const ResolvedProductPtr product = transformer()->product();
    const ProcessCommand * const cmd = processCommand();
    QString fullProgramPath = product->executablePathCache.value(cmd->program());
    if (!fullProgramPath.isEmpty())
        return fullProgramPath;

    fullProgramPath = cmd->program();
    if (logger().traceEnabled())
        logger().qbsTrace() << "[EXEC] looking for executable in PATH " << fullProgramPath;
    const QProcessEnvironment &buildEnvironment = product->buildEnvironment;
    QStringList pathEnv = buildEnvironment.value(QLatin1String("PATH"))
            .split(HostOsInfo::pathListSeparator(), QString::SkipEmptyParts);
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
    const ResolvedProductPtr product = transformer()->product();
    const ProcessCommand * const cmd = processCommand();
    QString fullProgramPath = product->executablePathCache.value(cmd->program());
    if (!fullProgramPath.isEmpty())
        return fullProgramPath;

    fullProgramPath = cmd->program();
    if (logger().traceEnabled())
        logger().qbsTrace() << "[EXEC] looking for executable by suffix " << fullProgramPath;
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
        if (logger().traceEnabled())
            logger().qbsTrace() << "[EXEC] candidate: " << candidate;
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
