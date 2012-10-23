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

#include "process.h"

#include <tools/hostosinfo.h>

#include <QProcess>
#include <QScriptEngine>
#include <QScriptValue>
#include <QTextStream>

namespace qbs {

void Process::init(QScriptValue extensionObject)
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
    return obj;
}

Process::~Process()
{
    delete qstream;
    delete qprocess;
    delete qenvironment;
}

Process::Process(QScriptContext *context)
{
    Q_UNUSED(context);
    Q_ASSERT(thisObject().engine() == engine());
    Process *t = this;

    t->qprocess = new QProcess;
    t->qenvironment = 0;
    t->qstream = new QTextStream(t->qprocess);
}

QString Process::getEnv(const QString &name)
{
    Q_ASSERT(thisObject().engine() == engine());
    Process *t = qscriptvalue_cast<Process*>(thisObject());

    QProcessEnvironment &environment = t->ensureEnvironment();
    return environment.value(name);
}

void Process::setEnv(const QString &name, const QString &value)
{
    Q_ASSERT(thisObject().engine() == engine());
    Process *t = qscriptvalue_cast<Process*>(thisObject());

    QProcessEnvironment &environment = t->ensureEnvironment();
    environment.insert(name, value);
}

bool Process::start(const QString &program, const QStringList &arguments)
{
    Q_ASSERT(thisObject().engine() == engine());
    Process *t = qscriptvalue_cast<Process*>(thisObject());

    if (t->qenvironment)
        t->qprocess->setProcessEnvironment(*t->qenvironment);
    t->qprocess->start(program, arguments);
    return t->qprocess->waitForStarted();
}

int Process::exec(const QString &program, const QStringList &arguments)
{
    Q_ASSERT(thisObject().engine() == engine());
    Process *t = qscriptvalue_cast<Process*>(thisObject());

    if (t->qenvironment)
        t->qprocess->setProcessEnvironment(*t->qenvironment);
    t->qprocess->start(program, arguments);
    if (!t->qprocess->waitForStarted())
        return -1;
    qprocess->closeWriteChannel();
    qprocess->waitForFinished();
    return qprocess->exitCode();
}

void Process::close()
{
    Q_ASSERT(thisObject().engine() == engine());
    Process *t = qscriptvalue_cast<Process*>(thisObject());
    delete t->qprocess;
    t->qprocess = 0;
    delete t->qenvironment;
    t->qenvironment = 0;
    delete t->qstream;
    t->qstream = 0;
}

void Process::closeWriteChannel()
{
    Q_ASSERT(thisObject().engine() == engine());
    Process *t = this;

    t->qprocess->closeWriteChannel();
}

bool Process::waitForFinished(int msecs)
{
    Q_ASSERT(thisObject().engine() == engine());
    Process *t = this;

    return t->qprocess->waitForFinished(msecs);
}

void Process::setCodec(const QString &codec)
{
    Q_ASSERT(thisObject().engine() == engine());
    Process *t = qscriptvalue_cast<Process*>(thisObject());
    if (!t->qstream)
        return;
    t->qstream->setCodec(qPrintable(codec));
}

QString Process::readLine()
{
    Process *t = qscriptvalue_cast<Process*>(thisObject());
    if (!t->qprocess)
        return QString();
    return t->qstream->readLine();
}

QString Process::readAll()
{
    Process *t = qscriptvalue_cast<Process*>(thisObject());
    if (!t->qprocess)
        return QString();
    return t->qstream->readAll();
}

bool Process::atEof() const
{
    Process *t = qscriptvalue_cast<Process*>(thisObject());
    if (!t->qstream)
        return true;
    return t->qstream->atEnd();
}

void Process::write(const QString &str)
{
    Process *t = qscriptvalue_cast<Process*>(thisObject());
    if (!t->qstream)
        return;
    (*t->qstream) << str;
}

void Process::writeLine(const QString &str)
{
    Process *t = qscriptvalue_cast<Process*>(thisObject());
    if (!t->qstream)
        return;
    (*t->qstream) << str;
    if (HostOsInfo::isWindowsHost())
        (*t->qstream) << '\r';
    (*t->qstream) << '\n';
}

QProcessEnvironment &Process::ensureEnvironment()
{
    if (!qenvironment)
        qenvironment = new QProcessEnvironment(QProcessEnvironment::systemEnvironment());
    return *qenvironment;
}

} // namespace qbs
