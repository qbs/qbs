/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "command.h"
#include <logging/translator.h>
#include <tools/hostosinfo.h>
#include <tools/persistence.h>
#include <tools/qbsassert.h>

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

bool AbstractCommand::equals(const AbstractCommand *other) const
{
    return m_description == other->m_description && m_highlight == other->m_highlight
            && m_silent == other->m_silent && type() == other->type()
            && m_properties == other->m_properties;
}

void AbstractCommand::fillFromScriptValue(const QScriptValue *scriptValue, const CodeLocation &codeLocation)
{
    m_description = scriptValue->property(QLatin1String("description")).toString();
    m_highlight = scriptValue->property(QLatin1String("highlight")).toString();
    m_silent = scriptValue->property(QLatin1String("silent")).toBool();
    m_codeLocation = codeLocation;

    m_predefinedProperties
            << QLatin1String("description")
            << QLatin1String("highlight")
            << QLatin1String("silent");
}

void AbstractCommand::load(PersistentPool &pool)
{
    m_description = pool.idLoadString();
    m_highlight = pool.idLoadString();
    pool.stream() >> m_silent;
    m_codeLocation.load(pool);
    m_properties = pool.loadVariantMap();
}

void AbstractCommand::store(PersistentPool &pool) const
{
    pool.storeString(m_description);
    pool.storeString(m_highlight);
    pool.stream() << m_silent;
    m_codeLocation.store(pool);
    pool.store(m_properties);
}

void AbstractCommand::applyCommandProperties(const QScriptValue *scriptValue)
{
    QScriptValueIterator it(*scriptValue);
    while (it.hasNext()) {
        it.next();
        if (m_predefinedProperties.contains(it.name()))
            continue;
        m_properties.insert(it.name(), it.value().toVariant());
    }
}

static QScriptValue js_CommandBase(QScriptContext *context, QScriptEngine *engine)
{
    QScriptValue cmd = context->thisObject();
    QBS_ASSERT(context->isCalledAsConstructor(), cmd = engine->newObject());
    cmd.setProperty(QLatin1String("description"),
                    engine->toScriptValue(AbstractCommand::defaultDescription()));
    cmd.setProperty(QLatin1String("highlight"),
                    engine->toScriptValue(AbstractCommand::defaultHighLight()));
    cmd.setProperty(QLatin1String("silent"),
                    engine->toScriptValue(AbstractCommand::defaultIsSilent()));
    return cmd;
}

static QScriptValue js_Command(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(!context->isCalledAsConstructor()))
        return context->throwError(Tr::tr("Command constructor called without new."));

    static ProcessCommandPtr commandPrototype = ProcessCommand::create();

    QScriptValue program = context->argument(0);
    if (program.isUndefined())
        program = engine->toScriptValue(commandPrototype->program());
    QScriptValue arguments = context->argument(1);
    if (arguments.isUndefined())
        arguments = engine->toScriptValue(commandPrototype->arguments());
    QScriptValue cmd = js_CommandBase(context, engine);
    cmd.setProperty(QLatin1String("className"),
                    engine->toScriptValue(QString::fromLatin1("Command")));
    cmd.setProperty(QLatin1String("program"), program);
    cmd.setProperty(QLatin1String("arguments"), arguments);
    cmd.setProperty(QLatin1String("workingDir"),
                    engine->toScriptValue(commandPrototype->workingDir()));
    cmd.setProperty(QLatin1String("maxExitCode"),
                    engine->toScriptValue(commandPrototype->maxExitCode()));
    cmd.setProperty(QLatin1String("stdoutFilterFunction"),
                    engine->toScriptValue(commandPrototype->stdoutFilterFunction()));
    cmd.setProperty(QLatin1String("stderrFilterFunction"),
                    engine->toScriptValue(commandPrototype->stderrFilterFunction()));
    cmd.setProperty(QLatin1String("responseFileThreshold"),
                    engine->toScriptValue(commandPrototype->responseFileThreshold()));
    cmd.setProperty(QLatin1String("responseFileUsagePrefix"),
                    engine->toScriptValue(commandPrototype->responseFileUsagePrefix()));
    cmd.setProperty(QLatin1String("environment"),
                    engine->toScriptValue(commandPrototype->environment().toStringList()));
    return cmd;
}


void ProcessCommand::setupForJavaScript(QScriptValue targetObject)
{
    QBS_CHECK(targetObject.isObject());
    QScriptValue ctor = targetObject.engine()->newFunction(js_Command, 2);
    targetObject.setProperty(QLatin1String("Command"), ctor);
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
    m_program = scriptValue->property(QLatin1String("program")).toString();
    m_arguments = scriptValue->property(QLatin1String("arguments")).toVariant().toStringList();
    m_workingDir = scriptValue->property(QLatin1String("workingDirectory")).toString();
    m_maxExitCode = scriptValue->property(QLatin1String("maxExitCode")).toInt32();
    QScriptValue stdoutFilterFunction =
            scriptValue->property(QLatin1String("stdoutFilterFunction")).toString();
    if (stdoutFilterFunction.isFunction())
        m_stdoutFilterFunction = QLatin1String("(") + stdoutFilterFunction.toString()
                + QLatin1String(")()");
    else
        m_stdoutFilterFunction = stdoutFilterFunction.toString();
    QScriptValue stderrFilterFunction =
            scriptValue->property(QLatin1String("stderrFilterFunction")).toString();
    if (stderrFilterFunction.isFunction())
        m_stderrFilterFunction = QLatin1String("(") + stderrFilterFunction.toString()
                + QLatin1String(")()");
    else
        m_stderrFilterFunction = stderrFilterFunction.toString();
    m_responseFileThreshold = scriptValue->property(QLatin1String("responseFileThreshold"))
            .toInt32();
    m_responseFileUsagePrefix = scriptValue->property(QLatin1String("responseFileUsagePrefix"))
            .toString();
    QStringList envList = scriptValue->property(QLatin1String("environment")).toVariant()
            .toStringList();
    getEnvironmentFromList(envList);

    m_predefinedProperties
            << QLatin1String("program")
            << QLatin1String("arguments")
            << QLatin1String("workingDirectory")
            << QLatin1String("maxExitCode")
            << QLatin1String("stdoutFilterFunction")
            << QLatin1String("stderrFilterFunction")
            << QLatin1String("responseFileThreshold")
            << QLatin1String("responseFileUsagePrefix")
            << QLatin1String("environment");
    applyCommandProperties(scriptValue);
}

void ProcessCommand::load(PersistentPool &pool)
{
    AbstractCommand::load(pool);
    m_program = pool.idLoadString();
    m_arguments = pool.idLoadStringList();
    const QStringList envList = pool.idLoadStringList();
    m_workingDir = pool.idLoadString();
    m_stdoutFilterFunction = pool.idLoadString();
    m_stderrFilterFunction = pool.idLoadString();
    m_responseFileUsagePrefix = pool.idLoadString();
    pool.stream() >> m_maxExitCode >> m_responseFileThreshold;
    getEnvironmentFromList(envList);
}

void ProcessCommand::store(PersistentPool &pool) const
{
    AbstractCommand::store(pool);
    pool.storeString(m_program);
    pool.storeStringList(m_arguments);
    pool.storeStringList(m_environment.toStringList());
    pool.storeString(m_workingDir);
    pool.storeString(m_stdoutFilterFunction);
    pool.storeString(m_stderrFilterFunction);
    pool.storeString(m_responseFileUsagePrefix);
    pool.stream() << m_maxExitCode << m_responseFileThreshold;
}

static QScriptValue js_JavaScriptCommand(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(!context->isCalledAsConstructor()))
        return context->throwError(Tr::tr("JavaScriptCommand constructor called without new."));
    if (Q_UNLIKELY(context->argumentCount() != 0)) {
        return context->throwError(QScriptContext::SyntaxError,
                                  QLatin1String("JavaScriptCommand c'tor doesn't take arguments."));
    }

    static JavaScriptCommandPtr commandPrototype = JavaScriptCommand::create();
    QScriptValue cmd = js_CommandBase(context, engine);
    cmd.setProperty(QLatin1String("className"),
                    engine->toScriptValue(QString::fromLatin1("JavaScriptCommand")));
    cmd.setProperty(QLatin1String("sourceCode"),
                    engine->toScriptValue(commandPrototype->sourceCode()));
    return cmd;
}

void JavaScriptCommand::setupForJavaScript(QScriptValue targetObject)
{
    QBS_CHECK(targetObject.isObject());
    QScriptValue ctor = targetObject.engine()->newFunction(js_JavaScriptCommand, 0);
    targetObject.setProperty(QLatin1String("JavaScriptCommand"), ctor);
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
    return m_sourceCode == other->m_sourceCode;
}

void JavaScriptCommand::fillFromScriptValue(const QScriptValue *scriptValue, const CodeLocation &codeLocation)
{
    AbstractCommand::fillFromScriptValue(scriptValue, codeLocation);
    QScriptValue sourceCode = scriptValue->property(QLatin1String("sourceCode"));
    if (sourceCode.isFunction())
        m_sourceCode = QLatin1String("(") + sourceCode.toString() + QLatin1String(")()");
    else
        m_sourceCode = sourceCode.toString();

    m_predefinedProperties << QLatin1String("className") << QLatin1String("sourceCode");
    applyCommandProperties(scriptValue);
}

void JavaScriptCommand::load(PersistentPool &pool)
{
    AbstractCommand::load(pool);
    m_sourceCode = pool.idLoadString();
}

void JavaScriptCommand::store(PersistentPool &pool) const
{
    AbstractCommand::store(pool);
    pool.storeString(m_sourceCode);
}

QList<AbstractCommandPtr> loadCommandList(PersistentPool &pool)
{
    QList<AbstractCommandPtr> commands;
    int count;
    pool.stream() >> count;
    commands.reserve(count);
    while (--count >= 0) {
        int cmdType;
        pool.stream() >> cmdType;
        AbstractCommandPtr cmd;
        switch (cmdType) {
        case AbstractCommand::JavaScriptCommandType:
            cmd = pool.idLoadS<JavaScriptCommand>();
            break;
        case AbstractCommand::ProcessCommandType:
            cmd = pool.idLoadS<ProcessCommand>();
            break;
        default:
            QBS_CHECK(false);
        }
        commands += cmd;
    }
    return commands;
}

void storeCommandList(const QList<AbstractCommandPtr> &commands, PersistentPool &pool)
{
    pool.stream() << commands.count();
    foreach (const AbstractCommandPtr &cmd, commands) {
        pool.stream() << int(cmd->type());
        pool.store(cmd);
    }
}

bool commandListsAreEqual(const QList<AbstractCommandPtr> &l1, const QList<AbstractCommandPtr> &l2)
{
    if (l1.count() != l2.count())
        return false;
    for (int i = 0; i < l1.count(); ++i)
        if (!l1.at(i)->equals(l2.at(i).data()))
            return false;
    return true;
}

} // namespace Internal
} // namespace qbs
