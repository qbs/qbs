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

#include "jsextension.h"

#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/executablefinder.h>
#include <tools/hostosinfo.h>
#include <tools/shellutils.h>
#include <tools/stringconstants.h>

#include <QtCore/qprocess.h>
#include <QtCore/qtextstream.h>
#include <QtCore/qvariant.h>
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#include <QtCore5Compat/qtextcodec.h>
#else
#include <QtCore/qtextcodec.h>
#endif

namespace qbs {
namespace Internal {

class Process : public JsExtension<Process>
{
    friend class JsExtension<Process>;
public:
    static const char *name() { return "Process"; }
    static JSValue ctor(JSContext *ctx, JSValueConst, JSValueConst, int, JSValueConst *, int);
    static void setupStaticMethods(JSContext *ctx, JSValue classObj);
    static void setupMethods(JSContext *ctx, JSValue obj);
    Process(JSContext *context);

    DEFINE_JS_FORWARDER(jsGetEnv, &Process::getEnv, "Process.getEnv")
    DEFINE_JS_FORWARDER(jsSetEnv, &Process::setEnv, "Process.setEnv")
    DEFINE_JS_FORWARDER(jsSetCodec, &Process::setCodec, "Process.setCodec")
    DEFINE_JS_FORWARDER(jsWorkingDir, &Process::workingDirectory, "Process.workingDirectory")
    DEFINE_JS_FORWARDER(jsSetWorkingDir, &Process::setWorkingDirectory,
                        "Process.setWorkingDirectory")
    DEFINE_JS_FORWARDER(jsStart, &Process::start, "Process.start")
    DEFINE_JS_FORWARDER(jsClose, &Process::close, "Process.close")
    DEFINE_JS_FORWARDER(jsTerminate, &Process::terminate, "Process.terminate")
    DEFINE_JS_FORWARDER(jsKill, &Process::kill, "Process.kill")
    DEFINE_JS_FORWARDER(jsReadLine, &Process::readLine, "Process.readLine")
    DEFINE_JS_FORWARDER(jsAtEnd, &Process::atEnd, "Process.atEnd")
    DEFINE_JS_FORWARDER(jsReadStdOut, &Process::readStdOut, "Process.readStdOut")
    DEFINE_JS_FORWARDER(jsReadStdErr, &Process::readStdErr, "Process.readStdErr")
    DEFINE_JS_FORWARDER(jsCloseWriteChannel, &Process::closeWriteChannel,
                        "Process.closeWriteChannel")
    DEFINE_JS_FORWARDER(jsWrite, &Process::write, "Process.write")
    DEFINE_JS_FORWARDER(jsWriteLine, &Process::writeLine, "Process.writeLine")
    DEFINE_JS_FORWARDER(jsExitCode, &Process::exitCode, "Process.exitCode")

    static JSValue jsExec(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
    {
        try {
            const auto args = getArguments<QString, QStringList>(ctx, "Process.exec", argc, argv);
            bool throwOnError = false;
            if (argc > 2)
                throwOnError = fromArg<bool>(ctx, "Process.exec", 3, argv[2]);
            return JS_NewInt32(ctx, fromJsObject(ctx, this_val)
                               ->exec(std::get<0>(args), std::get<1>(args), throwOnError));
        } catch (const QString &error) { return throwError(ctx, error); }
    }
    static JSValue jsWaitForFinished(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
    {
        try {
            int msecs = 30000;
            if (argc > 0)
                msecs = getArgument<int>(ctx, "Process.waitForFinished", argc, argv);
            return JS_NewBool(ctx, fromJsObject(ctx, this_val)->waitForFinished(msecs));
        } catch (const QString &error) { return throwError(ctx, error); }
    }
    QString getEnv(const QString &name) const { return m_environment.value(name); }
    void setEnv(const QString &name, const QString &value) { m_environment.insert(name, value); }
    void setCodec(const QString &codec);

    QString workingDirectory() const { return m_workingDirectory; }
    void setWorkingDirectory(const QString &dir) { m_workingDirectory = dir; }

    bool start(const QString &program, const QStringList &arguments);
    int exec(const QString &program, const QStringList &arguments, bool throwOnError);
    void close();
    bool waitForFinished(int msecs);
    void terminate() { m_qProcess->terminate(); }
    void kill() { m_qProcess->kill(); }

    QString readLine();
    bool atEnd() const { return m_qProcess->atEnd(); }
    QString readStdOut() { return m_codec->toUnicode(m_qProcess->readAllStandardOutput()); }
    QString readStdErr() { return m_codec->toUnicode(m_qProcess->readAllStandardError()); }

    void closeWriteChannel() { m_qProcess->closeWriteChannel(); }

    void write(const QString &str) { m_qProcess->write(m_codec->fromUnicode(str)); }
    void writeLine(const QString &str);

    int exitCode() const { return m_qProcess->exitCode(); }

    static JSValue jsShellQuote(JSContext *ctx, JSValue, int argc, JSValue *argv);

private:
    QString findExecutable(const QString &filePath) const;

    std::unique_ptr<QProcess> m_qProcess;
    QProcessEnvironment m_environment;
    QString m_workingDirectory;
    QTextCodec *m_codec = nullptr;
};

JSValue Process::ctor(JSContext *ctx, JSValueConst, JSValueConst, int, JSValueConst *, int)
{
    try {
        JSValue obj = createObject(ctx);

        Process * const process = fromJsObject(ctx, obj);
        const auto se = ScriptEngine::engineForContext(ctx);
        const DubiousContextList dubiousContexts{
            DubiousContext(EvalContext::PropertyEvaluation, DubiousContext::SuggestMoving)
        };
        se->checkContext(QStringLiteral("qbs.Process"), dubiousContexts);

        // Get environment
        QVariant v = se->property(StringConstants::qbsProcEnvVarInternal());
        if (v.isNull()) {
            // The build environment is not initialized yet.
            // This can happen if one uses Process on the RHS of a binding like Group.name.
            process->m_environment = se->environment();
        } else {
            process->m_environment
                = QProcessEnvironment(*reinterpret_cast<QProcessEnvironment*>(v.value<void*>()));
        }
        se->setUsesIo();
        return obj;
    } catch (const QString &error) { return throwError(ctx, error); }
}

void Process::setupStaticMethods(JSContext *ctx, JSValue classObj)
{
    setupMethod(ctx, classObj, "shellQuote", &Process::jsShellQuote, 3);
}

void Process::setupMethods(JSContext *ctx, JSValue obj)
{
    setupMethod(ctx, obj, "getEnv", &jsGetEnv, 1);
    setupMethod(ctx, obj, "setEnv", &jsSetEnv, 2);
    setupMethod(ctx, obj, "setCodec", &jsSetCodec, 1);
    setupMethod(ctx, obj, "workingDirectory", &jsWorkingDir, 0);
    setupMethod(ctx, obj, "setWorkingDirectory", &jsSetWorkingDir, 1);
    setupMethod(ctx, obj, "start", &jsStart, 2);
    setupMethod(ctx, obj, "exec", &jsExec, 3);
    setupMethod(ctx, obj, "close", &jsClose, 0);
    setupMethod(ctx, obj, "waitForFinished", &jsWaitForFinished, 1);
    setupMethod(ctx, obj, "terminate", &jsTerminate, 0);
    setupMethod(ctx, obj, "kill", &jsKill, 0);
    setupMethod(ctx, obj, "readLine", &jsReadLine, 0);
    setupMethod(ctx, obj, "atEnd", &jsAtEnd, 0);
    setupMethod(ctx, obj, "readStdOut", &jsReadStdOut, 0);
    setupMethod(ctx, obj, "readStdErr", &jsReadStdErr, 0);
    setupMethod(ctx, obj, "closeWriteChannel", &jsCloseWriteChannel, 0);
    setupMethod(ctx, obj, "write", &jsWrite, 1);
    setupMethod(ctx, obj, "writeLine", &jsWriteLine, 1);
    setupMethod(ctx, obj, "exitCode", &jsExitCode, 0);
}

Process::Process(JSContext *)
{
    m_qProcess = std::make_unique<QProcess>();
    m_codec = QTextCodec::codecForName("UTF-8");
}

bool Process::start(const QString &program, const QStringList &arguments)
{
    if (!m_workingDirectory.isEmpty())
        m_qProcess->setWorkingDirectory(m_workingDirectory);

    m_qProcess->setProcessEnvironment(m_environment);
    m_qProcess->start(findExecutable(program), arguments, QIODevice::ReadWrite | QIODevice::Text);
    return m_qProcess->waitForStarted();
}

int Process::exec(const QString &program, const QStringList &arguments, bool throwOnError)
{
    if (!start(findExecutable(program), arguments)) {
        if (throwOnError)
            throw Tr::tr("Error running '%1': %2").arg(program, m_qProcess->errorString());
        return -1;
    }
    m_qProcess->closeWriteChannel();
    m_qProcess->waitForFinished(-1);
    if (throwOnError) {
        if (m_qProcess->error() != QProcess::UnknownError
                && m_qProcess->error() != QProcess::Crashed) {
            throw Tr::tr("Error running '%1': %2").arg(program, m_qProcess->errorString());
        } else if (m_qProcess->exitStatus() == QProcess::CrashExit || m_qProcess->exitCode() != 0) {
            QString errorMessage = m_qProcess->error() == QProcess::Crashed
                    ? Tr::tr("Error running '%1': %2").arg(program, m_qProcess->errorString())
                    : Tr::tr("Process '%1 %2' finished with exit code %3.")
                                             .arg(program, arguments.join(QStringLiteral(" ")))
                                             .arg(m_qProcess->exitCode());
            const QString stdOut = readStdOut();
            if (!stdOut.isEmpty())
                errorMessage.append(Tr::tr(" The standard output was:\n")).append(stdOut);
            const QString stdErr = readStdErr();
            if (!stdErr.isEmpty())
                errorMessage.append(Tr::tr(" The standard error output was:\n")).append(stdErr);
            throw errorMessage;
        }
    }
    if (m_qProcess->error() != QProcess::UnknownError)
        return -1;
    return m_qProcess->exitCode();
}

void Process::close()
{
    if (!m_qProcess)
        return;
    m_qProcess.reset();
}

bool Process::waitForFinished(int msecs)
{
    if (m_qProcess->state() == QProcess::NotRunning)
        return true;
    return m_qProcess->waitForFinished(msecs);
}

void Process::setCodec(const QString &codec)
{
    const auto newCodec = QTextCodec::codecForName(qPrintable(codec));
    if (newCodec)
        m_codec = newCodec;
}

QString Process::readLine()
{
    auto result = m_codec->toUnicode(m_qProcess->readLine());
    if (!result.isEmpty() && result.back() == QLatin1Char('\n'))
        result.chop(1);
    return result;
}

QString Process::findExecutable(const QString &filePath) const
{
    ExecutableFinder exeFinder(ResolvedProductPtr(), m_environment);
    return exeFinder.findExecutable(filePath, m_workingDirectory);
}

void Process::writeLine(const QString &str)
{
    m_qProcess->write(m_codec->fromUnicode(str));
    m_qProcess->putChar('\n');
}

JSValue Process::jsShellQuote(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    try {
        const auto args = getArguments<QString, QStringList>(ctx, "Process.shellQuote", argc, argv);
        HostOsInfo::HostOs hostOs = HostOsInfo::hostOs();
        if (argc > 2) {
            const auto osList = fromArg<QStringList>(ctx, "Process.shellQuote", 3, argv[2]);
            hostOs = osList.contains(QLatin1String("windows"))
                    ? HostOsInfo::HostOsWindows : HostOsInfo::HostOsOtherUnix;
        }
        return makeJsString(ctx, shellQuote(std::get<0>(args), std::get<1>(args), hostOs));
    } catch (const QString &error) { return throwError(ctx, error); }
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionProcess(qbs::Internal::ScriptEngine *engine, JSValue extensionObject)
{
    qbs::Internal::Process::registerClass(engine, extensionObject);
}
