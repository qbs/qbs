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
#include "command.h"

#include <QDebug>
#include <QScriptEngine>
#include <QScriptValueIterator>

namespace qbs {

AbstractCommand::AbstractCommand()
    : m_silent(true)
{
}

AbstractCommand::~AbstractCommand()
{
}

AbstractCommand *AbstractCommand::createByType(AbstractCommand::CommandType commandType)
{
    switch (commandType) {
    case qbs::AbstractCommand::AbstractCommandType:
        break;
    case qbs::AbstractCommand::ProcessCommandType:
        return new ProcessCommand;
    case qbs::AbstractCommand::JavaScriptCommandType:
        return new JavaScriptCommand;
    }
    return 0;
}

void AbstractCommand::fillFromScriptValue(const QScriptValue *scriptValue, const CodeLocation &codeLocation)
{
    Q_UNUSED(codeLocation);
    m_description = scriptValue->property("description").toString();
    m_highlight = scriptValue->property("highlight").toString();
    m_silent = scriptValue->property("silent").toBool();
}

void AbstractCommand::load(QDataStream &s)
{
    s >> m_description >> m_highlight >> m_silent;
}

void AbstractCommand::store(QDataStream &s)
{
    s << m_description << m_highlight << m_silent;
}

static QScriptValue js_CommandBase(QScriptContext *context, QScriptEngine *engine)
{
    static AbstractCommand commandPrototype;
    QScriptValue cmd = context->thisObject();
    cmd.setProperty("description", engine->toScriptValue(commandPrototype.description()));
    cmd.setProperty("highlight", engine->toScriptValue(commandPrototype.highlight()));
    cmd.setProperty("silent", engine->toScriptValue(commandPrototype.isSilent()));
    return cmd;
}

static QScriptValue js_Command(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() != 2) {
        return context->throwError(QScriptContext::SyntaxError,
                                   "Command c'tor expects 2 arguments");
    }

    static ProcessCommand commandPrototype;

    QScriptValue program = context->argument(0);
    if (program.isUndefined())
        program = engine->toScriptValue(commandPrototype.program());
    QScriptValue arguments = context->argument(1);
    if (arguments.isUndefined())
        arguments = engine->toScriptValue(commandPrototype.arguments());
    QScriptValue cmd = js_CommandBase(context, engine);
    cmd.setProperty("className", engine->toScriptValue(QString("Command")));
    cmd.setProperty("program", program);
    cmd.setProperty("arguments", arguments);
    cmd.setProperty("workingDir", engine->toScriptValue(commandPrototype.workingDir()));
    cmd.setProperty("maxExitCode", engine->toScriptValue(commandPrototype.maxExitCode()));
    cmd.setProperty("stdoutFilterFunction", engine->toScriptValue(commandPrototype.stdoutFilterFunction()));
    cmd.setProperty("stderrFilterFunction", engine->toScriptValue(commandPrototype.stderrFilterFunction()));
    cmd.setProperty("responseFileThreshold", engine->toScriptValue(commandPrototype.responseFileThreshold()));
    cmd.setProperty("responseFileUsagePrefix", engine->toScriptValue(commandPrototype.responseFileUsagePrefix()));
    return cmd;
}


void ProcessCommand::setupForJavaScript(QScriptEngine *engine)
{
    QScriptValue ctor = engine->newFunction(js_Command, 2);
    engine->globalObject().setProperty("Command", ctor);
}

ProcessCommand::ProcessCommand()
    : m_maxExitCode(0),
      m_responseFileThreshold(32000)
{
}

void ProcessCommand::fillFromScriptValue(const QScriptValue *scriptValue, const CodeLocation &codeLocation)
{
    Q_UNUSED(codeLocation);
    AbstractCommand::fillFromScriptValue(scriptValue, codeLocation);
    m_program = scriptValue->property("program").toString();
    m_arguments = scriptValue->property("arguments").toVariant().toStringList();
    m_workingDir = scriptValue->property("workingDirectory").toString();
    m_maxExitCode = scriptValue->property("maxExitCode").toInt32();
    m_stdoutFilterFunction = scriptValue->property("stdoutFilterFunction").toString();
    m_stderrFilterFunction = scriptValue->property("stderrFilterFunction").toString();
    m_responseFileThreshold = scriptValue->property("responseFileThreshold").toInt32();
    m_responseFileUsagePrefix = scriptValue->property("responseFileUsagePrefix").toString();
}

void ProcessCommand::load(QDataStream &s)
{
    AbstractCommand::load(s);
    s   >> m_program
        >> m_arguments
        >> m_workingDir
        >> m_maxExitCode
        >> m_stdoutFilterFunction
        >> m_stderrFilterFunction
        >> m_responseFileThreshold
        >> m_responseFileUsagePrefix;
}

void ProcessCommand::store(QDataStream &s)
{
    AbstractCommand::store(s);
    s   << m_program
        << m_arguments
        << m_workingDir
        << m_maxExitCode
        << m_stdoutFilterFunction
        << m_stderrFilterFunction
        << m_responseFileThreshold
        << m_responseFileUsagePrefix;
}

static QScriptValue js_JavaScriptCommand(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() != 0) {
        return context->throwError(QScriptContext::SyntaxError,
                                   "JavaScriptCommand c'tor doesn't take arguments.");
    }

    static JavaScriptCommand commandPrototype;
    QScriptValue cmd = js_CommandBase(context, engine);
    cmd.setProperty("className", engine->toScriptValue(QString("JavaScriptCommand")));
    cmd.setProperty("sourceCode", engine->toScriptValue(commandPrototype.sourceCode()));
    return cmd;
}

void JavaScriptCommand::setupForJavaScript(QScriptEngine *engine)
{
    QScriptValue ctor = engine->newFunction(js_JavaScriptCommand, 0);
    engine->globalObject().setProperty("JavaScriptCommand", ctor);
}

JavaScriptCommand::JavaScriptCommand()
{
}

void JavaScriptCommand::fillFromScriptValue(const QScriptValue *scriptValue, const CodeLocation &codeLocation)
{
    m_codeLocation = codeLocation;
    AbstractCommand::fillFromScriptValue(scriptValue, codeLocation);
    QScriptValue sourceCode = scriptValue->property("sourceCode");
    if (sourceCode.isFunction()) {
        m_sourceCode = "(" + sourceCode.toString() + ")()";
    } else {
        m_sourceCode = sourceCode.toString();
    }
    static QSet<QString> predefinedProperties = QSet<QString>()
            << "description" << "highlight" << "silent" << "className" << "sourceCode";

    QScriptValueIterator it(*scriptValue);
    while (it.hasNext()) {
        it.next();
        if (predefinedProperties.contains(it.name()))
            continue;
        m_properties.insert(it.name(), it.value().toVariant());
    }
}

void JavaScriptCommand::load(QDataStream &s)
{
    AbstractCommand::load(s);
    s   >> m_sourceCode
        >> m_properties
        >> m_codeLocation;
}

void JavaScriptCommand::store(QDataStream &s)
{
    AbstractCommand::store(s);
    s   << m_sourceCode
        << m_properties
        << m_codeLocation;
}

} // namespace qbs
