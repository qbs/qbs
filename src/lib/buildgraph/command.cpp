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

#include "command.h"
#include <logging/translator.h>
#include <tools/qbsassert.h>
#include <tools/hostosinfo.h>

#include <QScriptEngine>
#include <QScriptValueIterator>
#include <QSet>

namespace qbs {
namespace Internal {

AbstractCommand::AbstractCommand()
    : m_description(defaultDescription()),
      m_highlight(defaultHighLight()),
      m_silent(defaultIsSilent())
{
}

AbstractCommand::~AbstractCommand()
{
}

AbstractCommand *AbstractCommand::createByType(AbstractCommand::CommandType commandType)
{
    switch (commandType) {
    case AbstractCommand::ProcessCommandType:
        return new ProcessCommand;
    case AbstractCommand::JavaScriptCommandType:
        return new JavaScriptCommand;
    }
    qFatal("%s: Invalid command type %d", Q_FUNC_INFO, commandType);
    return 0;
}

bool AbstractCommand::equals(const AbstractCommand *other) const
{
    return m_description == other->m_description && m_highlight == other->m_highlight
            && m_silent == other->m_silent && type() == other->type();
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
    QScriptValue cmd = context->thisObject();
    QBS_ASSERT(context->isCalledAsConstructor(), cmd = engine->newObject());
    cmd.setProperty("description", engine->toScriptValue(AbstractCommand::defaultDescription()));
    cmd.setProperty("highlight", engine->toScriptValue(AbstractCommand::defaultHighLight()));
    cmd.setProperty("silent", engine->toScriptValue(AbstractCommand::defaultIsSilent()));
    return cmd;
}

static QScriptValue js_Command(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(!context->isCalledAsConstructor()))
        return context->throwError(Tr::tr("Command constructor called without new."));
    if (Q_UNLIKELY(context->argumentCount() != 2)) {
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
    cmd.setProperty("environment", engine->toScriptValue(commandPrototype.environment().toStringList()));
    return cmd;
}


void ProcessCommand::setupForJavaScript(QScriptValue targetObject)
{
    QBS_CHECK(targetObject.isObject());
    QScriptValue ctor = targetObject.engine()->newFunction(js_Command, 2);
    targetObject.setProperty("Command", ctor);
}

ProcessCommand::ProcessCommand()
    : m_maxExitCode(0)
    , m_responseFileThreshold(HostOsInfo::isWindowsHost() ? 32000 : -1)
{
}

void ProcessCommand::getEnvironmentFromList(const QStringList &envList)
{
    m_environment.clear();
    foreach (const QString &env, envList) {
        const int equalsIndex = env.indexOf(QLatin1Char('='));
        if (equalsIndex <= 0 || equalsIndex == env.count() - 1)
            continue;
        const QString &var = env.left(equalsIndex);
        const QString &value = env.mid(equalsIndex + 1);
        m_environment.insert(var, value);
    }
}

bool ProcessCommand::equals(const AbstractCommand *otherAbstractCommand) const
{
    if (!AbstractCommand::equals(otherAbstractCommand))
        return false;
    const ProcessCommand * const other = static_cast<const ProcessCommand *>(otherAbstractCommand);
    return m_program == other->m_program
            && m_arguments == other->m_arguments
            && m_workingDir == other->m_workingDir
            && m_maxExitCode == other->m_maxExitCode
            && m_stdoutFilterFunction == other->m_stdoutFilterFunction
            && m_stderrFilterFunction == other->m_stderrFilterFunction
            && m_responseFileThreshold == other->m_responseFileThreshold
            && m_responseFileUsagePrefix == other->m_responseFileUsagePrefix
            && m_environment == other->m_environment;
}

void ProcessCommand::fillFromScriptValue(const QScriptValue *scriptValue, const CodeLocation &codeLocation)
{
    AbstractCommand::fillFromScriptValue(scriptValue, codeLocation);
    m_program = scriptValue->property("program").toString();
    m_arguments = scriptValue->property("arguments").toVariant().toStringList();
    m_workingDir = scriptValue->property("workingDirectory").toString();
    m_maxExitCode = scriptValue->property("maxExitCode").toInt32();
    m_stdoutFilterFunction = scriptValue->property("stdoutFilterFunction").toString();
    m_stderrFilterFunction = scriptValue->property("stderrFilterFunction").toString();
    m_responseFileThreshold = scriptValue->property("responseFileThreshold").toInt32();
    m_responseFileUsagePrefix = scriptValue->property("responseFileUsagePrefix").toString();
    QStringList envList = scriptValue->property(QLatin1String("environment")).toVariant()
            .toStringList();
    getEnvironmentFromList(envList);
}

void ProcessCommand::load(QDataStream &s)
{
    AbstractCommand::load(s);
    QStringList envList;
    s   >> m_program
        >> m_arguments
        >> envList
        >> m_workingDir
        >> m_maxExitCode
        >> m_stdoutFilterFunction
        >> m_stderrFilterFunction
        >> m_responseFileThreshold
        >> m_responseFileUsagePrefix;
    getEnvironmentFromList(envList);
}

void ProcessCommand::store(QDataStream &s)
{
    AbstractCommand::store(s);
    s   << m_program
        << m_arguments
        << m_environment.toStringList()
        << m_workingDir
        << m_maxExitCode
        << m_stdoutFilterFunction
        << m_stderrFilterFunction
        << m_responseFileThreshold
        << m_responseFileUsagePrefix;
}

static QScriptValue js_JavaScriptCommand(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(!context->isCalledAsConstructor()))
        return context->throwError(Tr::tr("JavaScriptCommand constructor called without new."));
    if (Q_UNLIKELY(context->argumentCount() != 0)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   "JavaScriptCommand c'tor doesn't take arguments.");
    }

    static JavaScriptCommand commandPrototype;
    QScriptValue cmd = js_CommandBase(context, engine);
    cmd.setProperty("className", engine->toScriptValue(QString("JavaScriptCommand")));
    cmd.setProperty("sourceCode", engine->toScriptValue(commandPrototype.sourceCode()));
    return cmd;
}

void JavaScriptCommand::setupForJavaScript(QScriptValue targetObject)
{
    QBS_CHECK(targetObject.isObject());
    QScriptValue ctor = targetObject.engine()->newFunction(js_JavaScriptCommand, 0);
    targetObject.setProperty("JavaScriptCommand", ctor);
}

JavaScriptCommand::JavaScriptCommand()
{
}

bool JavaScriptCommand::equals(const AbstractCommand *otherAbstractCommand) const
{
    if (!AbstractCommand::equals(otherAbstractCommand))
        return false;
    const JavaScriptCommand * const other
            = static_cast<const JavaScriptCommand *>(otherAbstractCommand);
    return m_sourceCode == other->m_sourceCode
            && m_properties == other->m_properties
            && m_codeLocation == other->m_codeLocation;
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

} // namespace Internal
} // namespace qbs
