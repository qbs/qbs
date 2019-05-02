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

#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/executablefinder.h>
#include <tools/hostosinfo.h>
#include <tools/shellutils.h>
#include <tools/stringconstants.h>

#include <QtCore/qobject.h>
#include <QtCore/qprocess.h>
#include <QtCore/qtextcodec.h>
#include <QtCore/qtextstream.h>
#include <QtCore/qvariant.h>

#include <QtScript/qscriptable.h>
#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptvalue.h>

namespace qbs {
namespace Internal {

class Process : public QObject, public QScriptable, public ResourceAcquiringScriptObject
{
    Q_OBJECT
public:
    static QScriptValue ctor(QScriptContext *context, QScriptEngine *engine);
    Process(QScriptContext *context);
    ~Process() override;

    Q_INVOKABLE QString getEnv(const QString &name);
    Q_INVOKABLE void setEnv(const QString &name, const QString &value);
    Q_INVOKABLE void setCodec(const QString &codec);

    Q_INVOKABLE QString workingDirectory();
    Q_INVOKABLE void setWorkingDirectory(const QString &dir);

    Q_INVOKABLE bool start(const QString &program, const QStringList &arguments);
    Q_INVOKABLE int exec(const QString &program, const QStringList &arguments,
                         bool throwOnError = false);
    Q_INVOKABLE void close();
    Q_INVOKABLE bool waitForFinished(int msecs = 30000);
    Q_INVOKABLE void terminate();
    Q_INVOKABLE void kill();

    Q_INVOKABLE QString readLine();
    Q_INVOKABLE bool atEnd() const;
    Q_INVOKABLE QString readStdOut();
    Q_INVOKABLE QString readStdErr();

    Q_INVOKABLE void closeWriteChannel();

    Q_INVOKABLE void write(const QString &str);
    Q_INVOKABLE void writeLine(const QString &str);

    Q_INVOKABLE int exitCode() const;

    static QScriptValue js_shellQuote(QScriptContext *context, QScriptEngine *engine);

private:
    QString findExecutable(const QString &filePath) const;

    // ResourceAcquiringScriptObject implementation
    void releaseResources() override;

    QProcess *m_qProcess;
    QProcessEnvironment m_environment;
    QString m_workingDirectory;
    QTextStream *m_textStream;
};

QScriptValue Process::ctor(QScriptContext *context, QScriptEngine *engine)
{
    Process *t;
    switch (context->argumentCount()) {
    case 0:
        t = new Process(context);
        break;
    default:
        return context->throwError(QStringLiteral("Process()"));
    }

    const auto se = static_cast<ScriptEngine *>(engine);
    se->addResourceAcquiringScriptObject(t);
    const DubiousContextList dubiousContexts ({
            DubiousContext(EvalContext::PropertyEvaluation, DubiousContext::SuggestMoving)
    });
    se->checkContext(QStringLiteral("qbs.Process"), dubiousContexts);

    QScriptValue obj = engine->newQObject(t, QScriptEngine::QtOwnership);

    // Get environment
    QVariant v = engine->property(StringConstants::qbsProcEnvVarInternal());
    if (v.isNull()) {
        // The build environment is not initialized yet.
        // This can happen if one uses Process on the RHS of a binding like Group.name.
        t->m_environment = static_cast<ScriptEngine *>(engine)->environment();
    } else {
        t->m_environment
            = QProcessEnvironment(*reinterpret_cast<QProcessEnvironment*>(v.value<void*>()));
    }
    se->setUsesIo();

    return obj;
}

Process::~Process()
{
    delete m_textStream;
    delete m_qProcess;
}

Process::Process(QScriptContext *context)
{
    Q_UNUSED(context);
    Q_ASSERT(thisObject().engine() == engine());

    m_qProcess = new QProcess;
    m_textStream = new QTextStream(m_qProcess);
}

QString Process::getEnv(const QString &name)
{
    Q_ASSERT(thisObject().engine() == engine());
    return m_environment.value(name);
}

void Process::setEnv(const QString &name, const QString &value)
{
    Q_ASSERT(thisObject().engine() == engine());
    m_environment.insert(name, value);
}

QString Process::workingDirectory()
{
    Q_ASSERT(thisObject().engine() == engine());
    return m_workingDirectory;
}

void Process::setWorkingDirectory(const QString &dir)
{
    Q_ASSERT(thisObject().engine() == engine());
    m_workingDirectory = dir;
}

bool Process::start(const QString &program, const QStringList &arguments)
{
    Q_ASSERT(thisObject().engine() == engine());

    if (!m_workingDirectory.isEmpty())
        m_qProcess->setWorkingDirectory(m_workingDirectory);

    m_qProcess->setProcessEnvironment(m_environment);
    m_qProcess->start(findExecutable(program), arguments);
    return m_qProcess->waitForStarted();
}

int Process::exec(const QString &program, const QStringList &arguments, bool throwOnError)
{
    Q_ASSERT(thisObject().engine() == engine());

    if (!start(findExecutable(program), arguments)) {
        if (throwOnError) {
            context()->throwError(Tr::tr("Error running '%1': %2")
                                  .arg(program, m_qProcess->errorString()));
        }
        return -1;
    }
    m_qProcess->closeWriteChannel();
    m_qProcess->waitForFinished(-1);
    if (throwOnError) {
        if (m_qProcess->error() != QProcess::UnknownError
                && m_qProcess->error() != QProcess::Crashed) {
            context()->throwError(Tr::tr("Error running '%1': %2")
                                  .arg(program, m_qProcess->errorString()));
        } else if (m_qProcess->exitStatus() == QProcess::CrashExit || m_qProcess->exitCode() != 0) {
            QString errorMessage = m_qProcess->error() == QProcess::Crashed
                    ? Tr::tr("Error running '%1': %2").arg(program, m_qProcess->errorString())
                    : Tr::tr("Process '%1' finished with exit code %2.").arg(program).arg(
                          m_qProcess->exitCode());
            const QString stdOut = readStdOut();
            if (!stdOut.isEmpty())
                errorMessage.append(Tr::tr(" The standard output was:\n")).append(stdOut);
            const QString stdErr = readStdErr();
            if (!stdErr.isEmpty())
                errorMessage.append(Tr::tr(" The standard error output was:\n")).append(stdErr);
            context()->throwError(errorMessage);
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
    Q_ASSERT(thisObject().engine() == engine());
    delete m_textStream;
    m_textStream = nullptr;
    delete m_qProcess;
    m_qProcess = nullptr;
}

bool Process::waitForFinished(int msecs)
{
    Q_ASSERT(thisObject().engine() == engine());

    if (m_qProcess->state() == QProcess::NotRunning)
        return true;
    return m_qProcess->waitForFinished(msecs);
}

void Process::terminate()
{
    m_qProcess->terminate();
}

void Process::kill()
{
    m_qProcess->kill();
}

void Process::setCodec(const QString &codec)
{
    Q_ASSERT(thisObject().engine() == engine());
    m_textStream->setCodec(qPrintable(codec));
}

QString Process::readLine()
{
    return m_textStream->readLine();
}

bool Process::atEnd() const
{
    return m_textStream->atEnd();
}

QString Process::readStdOut()
{
    return m_textStream->readAll();
}

QString Process::readStdErr()
{
    return m_textStream->codec()->toUnicode(m_qProcess->readAllStandardError());
}

void Process::closeWriteChannel()
{
    m_textStream->flush();
    m_qProcess->closeWriteChannel();
}

int Process::exitCode() const
{
    return m_qProcess->exitCode();
}

QString Process::findExecutable(const QString &filePath) const
{
    ExecutableFinder exeFinder(ResolvedProductPtr(), m_environment);
    return exeFinder.findExecutable(filePath, m_workingDirectory);
}

void Process::releaseResources()
{
    close();
    deleteLater();
}

void Process::write(const QString &str)
{
    (*m_textStream) << str;
}

void Process::writeLine(const QString &str)
{
    (*m_textStream) << str;
    if (HostOsInfo::isWindowsHost())
        (*m_textStream) << '\r';
    (*m_textStream) << '\n';
}

QScriptValue Process::js_shellQuote(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(context->argumentCount() < 2)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   QStringLiteral("shellQuote expects at least 2 arguments"));
    }
    const QString program = context->argument(0).toString();
    const QStringList args = context->argument(1).toVariant().toStringList();
    HostOsInfo::HostOs hostOs = HostOsInfo::hostOs();
    if (context->argumentCount() > 2) {
        hostOs = context->argument(2).toVariant().toStringList().contains(QLatin1String("windows"))
                ? HostOsInfo::HostOsWindows : HostOsInfo::HostOsOtherUnix;
    }
    return engine->toScriptValue(shellQuote(program, args, hostOs));
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionProcess(QScriptValue extensionObject)
{
    using namespace qbs::Internal;
    QScriptEngine *engine = extensionObject.engine();
    QScriptValue obj = engine->newQMetaObject(&Process::staticMetaObject, engine->newFunction(&Process::ctor));
    extensionObject.setProperty(QStringLiteral("Process"), obj);
    obj.setProperty(QStringLiteral("shellQuote"), engine->newFunction(Process::js_shellQuote, 3));
}

Q_DECLARE_METATYPE(qbs::Internal::Process *)

#include "process.moc"
