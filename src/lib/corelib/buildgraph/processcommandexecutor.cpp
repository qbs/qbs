/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "processcommandexecutor.h"

#include "artifact.h"
#include "rulecommands.h"
#include "transformer.h"

#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/commandechomode.h>
#include <tools/error.h>
#include <tools/executablefinder.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <tools/processresult.h>
#include <tools/processresult_p.h>
#include <tools/qbsassert.h>
#include <tools/scripttools.h>
#include <tools/shellutils.h>

#include <QtCore/qdir.h>
#include <QtCore/qtemporaryfile.h>
#include <QtCore/qtimer.h>

#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptvalue.h>

namespace qbs {
namespace Internal {

ProcessCommandExecutor::ProcessCommandExecutor(const Logger &logger, QObject *parent)
    : AbstractCommandExecutor(logger, parent)
{
    connect(&m_process, static_cast<void (QbsProcess::*)(QProcess::ProcessError)>(&QbsProcess::error),
            this, &ProcessCommandExecutor::onProcessError);
    connect(&m_process, static_cast<void (QbsProcess::*)(int)>(&QbsProcess::finished),
            this, &ProcessCommandExecutor::onProcessFinished);
}

void ProcessCommandExecutor::doSetup()
{
    const ProcessCommand * const cmd = processCommand();
    const QString program = ExecutableFinder(transformer()->product(),
                                             transformer()->product()->buildEnvironment, logger())
            .findExecutable(cmd->program(), cmd->workingDir());

    QProcessEnvironment env = m_buildEnvironment;
    const QProcessEnvironment &additionalVariables = cmd->environment();
    for (const QString &key : additionalVariables.keys())
        env.insert(key, additionalVariables.value(key));
    m_commandEnvironment = env;

    m_program = program;
    m_arguments = cmd->arguments();
    m_shellInvocation = shellQuote(QDir::toNativeSeparators(m_program), m_arguments);
}

void ProcessCommandExecutor::doStart()
{
    QBS_ASSERT(m_process.state() == QProcess::NotRunning, return);

    const ProcessCommand * const cmd = processCommand();

    m_process.setProcessEnvironment(m_commandEnvironment);

    QStringList arguments = m_arguments;

    if (dryRun() && !cmd->ignoreDryRun()) {
        QTimer::singleShot(0, this, [this] { emit finished(); }); // Don't call back on the caller.
        return;
    }

    const QString workingDir = QDir::fromNativeSeparators(cmd->workingDir());
    if (!workingDir.isEmpty()) {
        FileInfo fi(workingDir);
        if (!fi.exists() || !fi.isDir()) {
            emit finished(ErrorInfo(Tr::tr("The working directory '%1' for process '%2' "
                    "is invalid.").arg(QDir::toNativeSeparators(workingDir),
                                       QDir::toNativeSeparators(m_program)),
                                    cmd->codeLocation()));
            return;
        }
    }

    // Automatically use response files, if the command line gets to long.
    if (!cmd->responseFileUsagePrefix().isEmpty()) {
        const int commandLineLength = m_shellInvocation.length();
        if (cmd->responseFileThreshold() >= 0 && commandLineLength > cmd->responseFileThreshold()) {
            if (logger().debugEnabled()) {
                logger().qbsDebug() << QString::fromLatin1("[EXEC] Using response file. "
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
            for (int i = cmd->responseFileArgumentIndex(); i < cmd->arguments().count(); ++i) {
                const QString arg = cmd->arguments().at(i);
                if (arg.startsWith(cmd->responseFileUsagePrefix())) {
                    QFile f(arg.mid(cmd->responseFileUsagePrefix().count()));
                    if (!f.open(QIODevice::ReadOnly)) {
                        emit finished(ErrorInfo(Tr::tr("Cannot open command file '%1'.")
                                                .arg(QDir::toNativeSeparators(f.fileName()))));
                        return;
                    }
                    responseFile.write(f.readAll());
                } else {
                    responseFile.write(qbs::Internal::shellQuote(arg).toLocal8Bit());
                }
                responseFile.write("\n");
            }
            responseFile.close();
            m_responseFileName = responseFile.fileName();
            arguments = arguments.mid(0,
                                      std::min(cmd->responseFileArgumentIndex(), arguments.size()));
            arguments += QDir::toNativeSeparators(cmd->responseFileUsagePrefix()
                    + responseFile.fileName());
        }
    }

    if (logger().debugEnabled()) {
        logger().qbsDebug() << "[EXEC] Running external process; full command line is: "
                            << m_shellInvocation;
    }
    const QProcessEnvironment &additionalVariables = cmd->environment();
    if (logger().traceEnabled()) {
        logger().qbsTrace() << "[EXEC] Additional environment:"
                            << additionalVariables.toStringList();
    }
    m_process.setWorkingDirectory(workingDir);
    m_process.start(m_program, arguments);
}

void ProcessCommandExecutor::cancel()
{
    // We don't want this command to be reported as failing, since we explicitly terminated it.
    disconnect(this, &ProcessCommandExecutor::reportProcessResult, 0, 0);

    m_process.cancel();
}

QString ProcessCommandExecutor::filterProcessOutput(const QByteArray &_output,
        const QString &filterFunctionSource)
{
    const QString output = QString::fromLocal8Bit(_output);
    if (filterFunctionSource.isEmpty())
        return output;

    QScriptValue scope = scriptEngine()->newObject();
    scope.setPrototype(scriptEngine()->globalObject());
    for (QVariantMap::const_iterator it = command()->properties().constBegin();
            it != command()->properties().constEnd(); ++it) {
        scope.setProperty(it.key(), scriptEngine()->toScriptValue(it.value()));
    }

    TemporaryGlobalObjectSetter tgos(scope);
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
                         .arg(scriptEngine()->lastErrorString(filteredOutput)),
                                        scriptEngine()->lastErrorLocation(filteredOutput)));
        return output;
    }

    return filteredOutput.toString();
}

static QProcess::ProcessError saveToFile(const QString &filePath, const QByteArray &content)
{
    QBS_ASSERT(!filePath.isEmpty(), return QProcess::WriteError);

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly))
        return QProcess::WriteError;

    if (f.write(content) != content.size())
        return QProcess::WriteError;
    f.close();
    return f.error() == QFileDevice::NoError ? QProcess::UnknownError : QProcess::WriteError;
}

void ProcessCommandExecutor::getProcessOutput(bool stdOut, ProcessResult &result)
{
    QByteArray content;
    QString filterFunction;
    QString redirectPath;
    QStringList *target;
    if (stdOut) {
        content = m_process.readAllStandardOutput();
        filterFunction = processCommand()->stdoutFilterFunction();
        redirectPath = processCommand()->stdoutFilePath();
        target = &result.d->stdOut;
    } else {
        content = m_process.readAllStandardError();
        filterFunction = processCommand()->stderrFilterFunction();
        redirectPath = processCommand()->stderrFilePath();
        target = &result.d->stdErr;
    }
    QString contentString = filterProcessOutput(content, filterFunction);
    if (!redirectPath.isEmpty()) {
        const QProcess::ProcessError error = saveToFile(redirectPath, contentString.toLocal8Bit());
        if (result.error() == QProcess::UnknownError && error != QProcess::UnknownError)
            result.d->error = error;
    } else {
        if (!contentString.isEmpty() && contentString.endsWith(QLatin1Char('\n')))
            contentString.chop(1);
        *target = contentString.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    }
}

void ProcessCommandExecutor::sendProcessOutput()
{
    ProcessResult result;
    result.d->executableFilePath = m_program;
    result.d->arguments = m_arguments;
    result.d->workingDirectory = m_process.workingDirectory();
    if (result.workingDirectory().isEmpty())
        result.d->workingDirectory = QDir::currentPath();
    result.d->exitCode = m_process.exitCode();
    result.d->error = m_process.error();
    QString errorString = m_process.errorString();

    getProcessOutput(true, result);
    getProcessOutput(false, result);

    const bool processError = result.error() != QProcess::UnknownError;
    const bool failureExit = quint32(m_process.exitCode())
            > quint32(processCommand()->maxExitCode());
    result.d->success = !processError && !failureExit;
    emit reportProcessResult(result);

    if (Q_UNLIKELY(processError)) {
        emit finished(ErrorInfo(errorString));
    } else if (Q_UNLIKELY(failureExit)) {
        emit finished(ErrorInfo(Tr::tr("Process failed with exit code %1.")
                                .arg(m_process.exitCode())));
    } else {
        emit finished();
    }
}

void ProcessCommandExecutor::onProcessError()
{
    switch (m_process.error()) {
    case QProcess::FailedToStart: {
        removeResponseFile();
        const QString binary = QDir::toNativeSeparators(processCommand()->program());
        QString errorPrefixString;
#ifdef Q_OS_UNIX
        if (QFileInfo(binary).isExecutable()) {
            const QString interpreter(shellInterpreter(binary));
            if (!interpreter.isEmpty()) {
                errorPrefixString = Tr::tr("%1: bad interpreter: ").arg(interpreter);
            }
        }
#endif
        emit finished(ErrorInfo(Tr::tr("The process '%1' could not be started: %2. "
                                       "The full command line invocation was: %3")
                                .arg(binary, errorPrefixString + m_process.errorString(),
                                     m_shellInvocation)));
        break;
    }
    case QProcess::Crashed:
        break; // Ignore. Will be handled by onProcessFinished().
    default:
        logger().qbsWarning() << "QProcess error: " << m_process.errorString();
    }
}

void ProcessCommandExecutor::onProcessFinished()
{
    removeResponseFile();
    sendProcessOutput();
}

static QString environmentVariableString(const QString &key, const QString &value)
{
    QString str;
    if (HostOsInfo::isWindowsHost())
        str += QStringLiteral("set ");
    str += shellQuote(key + QLatin1Char('=') + value);
    if (HostOsInfo::isWindowsHost())
        str += QLatin1Char('\n');
    else
        str += QLatin1Char(' ');
    return str;
}

void ProcessCommandExecutor::doReportCommandDescription()
{
    if (m_echoMode == CommandEchoModeCommandLine ||
            m_echoMode == CommandEchoModeCommandLineWithEnvironment) {
        QString fullInvocation;
        if (m_echoMode == CommandEchoModeCommandLineWithEnvironment) {
            QStringList keys = m_commandEnvironment.keys();
            keys.sort();
            for (const QString &key : keys)
                fullInvocation += environmentVariableString(key, m_commandEnvironment.value(key));
        }
        fullInvocation += m_shellInvocation;
        emit reportCommandDescription(command()->highlight(),
                                      !command()->extendedDescription().isEmpty()
                                          ? command()->extendedDescription()
                                          : fullInvocation);
        return;
    }

    AbstractCommandExecutor::doReportCommandDescription();
}

void ProcessCommandExecutor::removeResponseFile()
{
    if (m_responseFileName.isEmpty())
        return;
    QFile::remove(m_responseFileName);
    m_responseFileName.clear();
}

const ProcessCommand *ProcessCommandExecutor::processCommand() const
{
    return static_cast<const ProcessCommand *>(command());
}

} // namespace Internal
} // namespace qbs
