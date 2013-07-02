/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#include "process.h"

#include <logging/translator.h>
#include <tools/hostosinfo.h>

#include <QProcess>
#include <QScriptEngine>
#include <QScriptValue>
#include <QTextCodec>
#include <QTextStream>

namespace qbs {
namespace Internal {

void initializeJsExtensionProcess(QScriptValue extensionObject)
{
    QScriptEngine *engine = extensionObject.engine();
    QScriptValue obj = engine->newQMetaObject(&Process::staticMetaObject, engine->newFunction(&Process::ctor));
    extensionObject.setProperty("Process", obj);
}

QScriptValue Process::ctor(QScriptContext *context, QScriptEngine *engine)
{
    Process *t;
    switch (context->argumentCount()) {
    case 0:
        t = new Process(context);
        break;
    default:
        return context->throwError("Process()");
    }

    QScriptValue obj = engine->newQObject(t, QScriptEngine::ScriptOwnership);

    // Get environment
    QVariant v = engine->property("_qbs_procenv");
    if (!v.isNull())
        t->m_environment
            = QProcessEnvironment(*reinterpret_cast<QProcessEnvironment*>(v.value<void*>()));

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

bool Process::start(const QString &program, const QStringList &arguments)
{
    Q_ASSERT(thisObject().engine() == engine());

    m_qProcess->setProcessEnvironment(m_environment);
    m_qProcess->start(program, arguments);
    return m_qProcess->waitForStarted();
}

int Process::exec(const QString &program, const QStringList &arguments, bool throwOnError)
{
    Q_ASSERT(thisObject().engine() == engine());

    m_qProcess->setProcessEnvironment(m_environment);
    m_qProcess->start(program, arguments);
    if (!m_qProcess->waitForStarted()) {
        if (throwOnError) {
            context()->throwError(Tr::tr("Error running '%1': %2")
                                  .arg(program, m_qProcess->errorString()));
        }
        return -1;
    }
    m_qProcess->closeWriteChannel();
    m_qProcess->waitForFinished();
    if (throwOnError) {
        if (m_qProcess->error() != QProcess::UnknownError) {
            context()->throwError(Tr::tr("Error running '%1': %2")
                                  .arg(program, m_qProcess->errorString()));
        } else if (m_qProcess->exitCode() != 0) {
            QString errorMessage = Tr::tr("Process '%1' finished with exit code %2.")
                    .arg(program).arg(m_qProcess->exitCode());
            const QString stdErr = readStdErr();
            if (!stdErr.isEmpty())
                errorMessage.append(Tr::tr(" The standard error output was:\n")).append(stdErr);
            context()->throwError(errorMessage);
        }
    }
    return m_qProcess->exitCode();
}

void Process::close()
{
    Q_ASSERT(thisObject().engine() == engine());
    delete m_qProcess;
    m_qProcess = 0;
    delete m_textStream;
    m_textStream = 0;
}

bool Process::waitForFinished(int msecs)
{
    Q_ASSERT(thisObject().engine() == engine());

    return m_qProcess->waitForFinished(msecs);
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

QString Process::readStdOut()
{
    return m_textStream->readAll();
}

QString Process::readStdErr()
{
    return m_textStream->codec()->toUnicode(m_qProcess->readAllStandardError());
}

int Process::exitCode() const
{
    return m_qProcess->exitCode();
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

} // namespace Internal
} // namespace qbs
