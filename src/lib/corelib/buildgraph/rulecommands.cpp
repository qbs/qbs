/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2019 Jochen Ulrich <jochenulrich@t-online.de>
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

#include "rulecommands.h"

#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <tools/qbsassert.h>
#include <tools/stringconstants.h>

#include <QtCore/qfile.h>

namespace qbs {
namespace Internal {

static QString argumentsProperty() { return QStringLiteral("arguments"); }
static QString environmentProperty() { return QStringLiteral("environment"); }
static QString extendedDescriptionProperty() { return QStringLiteral("extendedDescription"); }
static QString highlightProperty() { return QStringLiteral("highlight"); }
static QString ignoreDryRunProperty() { return QStringLiteral("ignoreDryRun"); }
static QString maxExitCodeProperty() { return QStringLiteral("maxExitCode"); }
static QString programProperty() { return QStringLiteral("program"); }
static QString responseFileArgumentIndexProperty()
{
    return QStringLiteral("responseFileArgumentIndex");
}
static QString responseFileThresholdProperty() { return QStringLiteral("responseFileThreshold"); }
static QString responseFileUsagePrefixProperty()
{
    return QStringLiteral("responseFileUsagePrefix");
}
static QString responseFileSeparatorProperty() { return QStringLiteral("responseFileSeparator"); }
static QString silentProperty() { return QStringLiteral("silent"); }
static QString stderrFilePathProperty() { return QStringLiteral("stderrFilePath"); }
static QString stderrFilterFunctionProperty() { return QStringLiteral("stderrFilterFunction"); }
static QString stdoutFilePathProperty() { return QStringLiteral("stdoutFilePath"); }
static QString stdoutFilterFunctionProperty() { return QStringLiteral("stdoutFilterFunction"); }
static QString timeoutProperty() { return QStringLiteral("timeout"); }
static QString workingDirProperty() { return QStringLiteral("workingDirectory"); }

static QString invokedSourceCode(JSContext *ctx, const JSValue &codeOrFunction)
{
    const QString &code = getJsString(ctx, codeOrFunction);
    return JS_IsFunction(ctx, codeOrFunction)
            ? QStringLiteral("(") + code + QStringLiteral(")()") : code;
}

AbstractCommand::AbstractCommand()
    : m_description(defaultDescription()),
      m_extendedDescription(defaultExtendedDescription()),
      m_highlight(defaultHighLight()),
      m_ignoreDryRun(defaultIgnoreDryRun()),
      m_silent(defaultIsSilent()),
      m_timeout(defaultTimeout())
{
}

AbstractCommand::~AbstractCommand() = default;

bool AbstractCommand::equals(const AbstractCommand *other) const
{
    return type() == other->type() && m_description == other->m_description
           && m_extendedDescription == other->m_extendedDescription
           && m_highlight == other->m_highlight && m_ignoreDryRun == other->m_ignoreDryRun
           && m_silent == other->m_silent && m_jobPool == other->m_jobPool
           && m_timeout == other->m_timeout && qVariantMapsEqual(m_properties, other->m_properties);
}

void AbstractCommand::fillFromScriptValue(JSContext *ctx, const JSValue *scriptValue,
                                          const CodeLocation &codeLocation)
{
    m_description = getJsStringProperty(ctx, *scriptValue, StringConstants::descriptionProperty());
    m_extendedDescription = getJsStringProperty(ctx, *scriptValue, extendedDescriptionProperty());
    m_highlight = getJsStringProperty(ctx, *scriptValue, highlightProperty());
    m_ignoreDryRun = getJsBoolProperty(ctx, *scriptValue, ignoreDryRunProperty());
    m_silent = getJsBoolProperty(ctx, *scriptValue, silentProperty());
    m_jobPool = getJsStringProperty(ctx, *scriptValue, StringConstants::jobPoolProperty());
    const auto timeoutScriptValue = getJsProperty(ctx, *scriptValue, timeoutProperty());
    if (!JS_IsUndefined(timeoutScriptValue) && !JS_IsNull(timeoutScriptValue))
        m_timeout = JS_VALUE_GET_INT(timeoutScriptValue);
    m_codeLocation = codeLocation;

    m_predefinedProperties
            << StringConstants::descriptionProperty()
            << extendedDescriptionProperty()
            << highlightProperty()
            << ignoreDryRunProperty()
            << StringConstants::jobPoolProperty()
            << silentProperty()
            << timeoutProperty();
}

QString AbstractCommand::fullDescription(const QString &productName) const
{
    return QLatin1Char('[') + productName + QLatin1String("] ") + description();
}

QString AbstractCommand::descriptionForCancelMessage(const QString &productName) const
{
    return fullDescription(productName);
}

void AbstractCommand::load(PersistentPool &pool)
{
    serializationOp<PersistentPool::Load>(pool);
}

void AbstractCommand::store(PersistentPool &pool)
{
    serializationOp<PersistentPool::Store>(pool);
}

void AbstractCommand::applyCommandProperties(JSContext *ctx, const JSValue *scriptValue)
{
    handleJsProperties(ctx, *scriptValue, [this, ctx](const JSAtom &prop, const JSPropertyDescriptor &desc) {
        const QString name = getJsString(ctx, prop);
        if (m_predefinedProperties.contains(name))
            return;
        // TODO: Use script class for command objects, don't allow setting random properties
        if (!JS_IsSimpleValue(ctx, desc.value)) {
            throw ErrorInfo(Tr::tr("Property '%1' has a type unsuitable for storing in a command "
                                   "object.").arg(name), m_codeLocation);
        }
        m_properties.insert(name, getJsVariant(ctx, desc.value));
    });
}

static JSValue js_CommandBase(JSContext *ctx)
{
    const JSValue cmd = JS_NewObject(ctx);
    setJsProperty(ctx, cmd, StringConstants::descriptionProperty(),
                  makeJsString(ctx, AbstractCommand::defaultDescription()));
    setJsProperty(ctx, cmd, extendedDescriptionProperty(),
                  makeJsString(ctx, AbstractCommand::defaultExtendedDescription()));
    setJsProperty(ctx, cmd, highlightProperty(),
                  makeJsString(ctx, AbstractCommand::defaultHighLight()));
    setJsProperty(ctx, cmd, ignoreDryRunProperty(),
                  JS_NewBool(ctx, AbstractCommand::defaultIgnoreDryRun()));
    setJsProperty(ctx, cmd, silentProperty(),
                  JS_NewBool(ctx, AbstractCommand::defaultIsSilent()));
    setJsProperty(ctx, cmd, timeoutProperty(),
                  JS_NewInt32(ctx, AbstractCommand::defaultTimeout()));
    return cmd;
}

static JSValue js_Command(JSContext *ctx, JSValueConst, JSValueConst,
                          int argc, JSValueConst *argv, int)
{
    static ProcessCommandPtr commandPrototype = ProcessCommand::create();
    JSValue program = JS_UNDEFINED;
    if (argc > 0)
        program = JS_DupValue(ctx, argv[0]);
    if (JS_IsUndefined(program))
        program = makeJsString(ctx, commandPrototype->program());
    JSValue arguments = JS_UNDEFINED;
    if (argc > 1)
        arguments = JS_DupValue(ctx, argv[1]);
    if (JS_IsUndefined(arguments))
        arguments = makeJsStringList(ctx, commandPrototype->arguments());
    if (JS_IsString(arguments)) {
        JSValue args = JS_NewArray(ctx);
        JS_SetPropertyUint32(ctx, args, 0, arguments);
        arguments = args;
    }
    JSValue cmd = js_CommandBase(ctx);
    setJsProperty(ctx, cmd, StringConstants::classNameProperty(),
                  makeJsString(ctx, StringConstants::commandType()));
    setJsProperty(ctx, cmd, programProperty(), program);
    setJsProperty(ctx, cmd, argumentsProperty(), arguments);
    setJsProperty(ctx, cmd, workingDirProperty(),
                  makeJsString(ctx, commandPrototype->workingDir()));
    setJsProperty(ctx, cmd, maxExitCodeProperty(),
                  JS_NewInt32(ctx, commandPrototype->maxExitCode()));
    setJsProperty(ctx, cmd, stdoutFilterFunctionProperty(),
                  makeJsString(ctx, commandPrototype->stdoutFilterFunction()));
    setJsProperty(ctx, cmd, stderrFilterFunctionProperty(),
                  makeJsString(ctx, commandPrototype->stderrFilterFunction()));
    setJsProperty(ctx, cmd, responseFileThresholdProperty(),
                  JS_NewInt32(ctx, commandPrototype->responseFileThreshold()));
    setJsProperty(ctx, cmd, responseFileArgumentIndexProperty(),
                  JS_NewInt32(ctx, commandPrototype->responseFileArgumentIndex()));
    setJsProperty(ctx, cmd, responseFileUsagePrefixProperty(),
                  makeJsString(ctx, commandPrototype->responseFileUsagePrefix()));
    setJsProperty(ctx, cmd, responseFileSeparatorProperty(),
                  makeJsString(ctx, commandPrototype->responseFileSeparator()));
    setJsProperty(ctx, cmd, stdoutFilePathProperty(),
                  makeJsString(ctx, commandPrototype->stdoutFilePath()));
    setJsProperty(ctx, cmd, stderrFilePathProperty(),
                  makeJsString(ctx, commandPrototype->stderrFilePath()));
    setJsProperty(ctx, cmd, environmentProperty(),
                  makeJsStringList(ctx, commandPrototype->environment().toStringList()));
    setJsProperty(ctx, cmd, ignoreDryRunProperty(),
                  JS_NewBool(ctx, commandPrototype->ignoreDryRun()));
    return cmd;
}


void ProcessCommand::setupForJavaScript(ScriptEngine *engine, JSValue targetObject)
{
    engine->registerClass(StringConstants::commandType().toUtf8().constData(), js_Command, nullptr,
                          targetObject);
}

ProcessCommand::ProcessCommand()
    : m_maxExitCode(0)
    , m_responseFileThreshold(defaultResponseFileThreshold())
    , m_responseFileArgumentIndex(0)
    , m_responseFileSeparator(QStringLiteral("\n"))
{
}

int ProcessCommand::defaultResponseFileThreshold() const
{
    // TODO: Non-Windows platforms likely have their own limits. Investigate.
    return HostOsInfo::isWindowsHost()
            ? 31000 // 32000 minus "safety offset"
            : -1;
}

void ProcessCommand::getEnvironmentFromList(const QStringList &envList)
{
    m_environment.clear();
    for (const QString &env : envList) {
        const int equalsIndex = env.indexOf(QLatin1Char('='));
        if (equalsIndex <= 0 || equalsIndex == env.size() - 1)
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
    const auto other = static_cast<const ProcessCommand *>(otherAbstractCommand);
    return m_program == other->m_program
            && m_arguments == other->m_arguments
            && m_workingDir == other->m_workingDir
            && m_maxExitCode == other->m_maxExitCode
            && m_stdoutFilterFunction == other->m_stdoutFilterFunction
            && m_stderrFilterFunction == other->m_stderrFilterFunction
            && m_responseFileThreshold == other->m_responseFileThreshold
            && m_responseFileArgumentIndex == other->m_responseFileArgumentIndex
            && m_responseFileUsagePrefix == other->m_responseFileUsagePrefix
            && m_responseFileSeparator == other->m_responseFileSeparator
            && m_stdoutFilePath == other->m_stdoutFilePath
            && m_stderrFilePath == other->m_stderrFilePath
            && m_relevantEnvVars == other->m_relevantEnvVars
            && m_relevantEnvValues == other->m_relevantEnvValues
            && m_environment == other->m_environment;
}

void ProcessCommand::fillFromScriptValue(JSContext *ctx, const JSValue *scriptValue, const CodeLocation &codeLocation)
{
    AbstractCommand::fillFromScriptValue(ctx, scriptValue, codeLocation);
    m_program = getJsStringProperty(ctx, *scriptValue, programProperty());
    m_arguments = getJsStringListProperty(ctx, *scriptValue, argumentsProperty());
    m_workingDir = getJsStringProperty(ctx, *scriptValue, workingDirProperty());
    m_maxExitCode = getJsIntProperty(ctx, *scriptValue, maxExitCodeProperty());

    const ScopedJsValue stdoutFilterFunction(ctx, getJsProperty(ctx, *scriptValue,
                                                                stdoutFilterFunctionProperty()));
    if (JS_IsFunction(ctx, stdoutFilterFunction))
        m_stdoutFilterFunction = QLatin1Char('(') + getJsString(ctx, stdoutFilterFunction) + QLatin1Char(')');

    const ScopedJsValue stderrFilterFunction(ctx, getJsProperty(ctx, *scriptValue,
                                                                stderrFilterFunctionProperty()));
    if (JS_IsFunction(ctx, stderrFilterFunction))
        m_stderrFilterFunction = QLatin1Char('(') + getJsString(ctx, stderrFilterFunction) + QLatin1Char(')');

    m_relevantEnvVars = getJsStringListProperty(ctx, *scriptValue,
                                                QStringLiteral("relevantEnvironmentVariables"));
    m_responseFileThreshold = getJsIntProperty(ctx, *scriptValue, responseFileThresholdProperty());
    m_responseFileArgumentIndex = getJsIntProperty(ctx, *scriptValue,
                                                   responseFileArgumentIndexProperty());
    m_responseFileUsagePrefix = getJsStringProperty(ctx, *scriptValue,
                                                    responseFileUsagePrefixProperty());
    m_responseFileSeparator = getJsStringProperty(ctx, *scriptValue,
                                                  responseFileSeparatorProperty());
    QStringList envList = getJsStringListProperty(ctx, *scriptValue, environmentProperty());
    getEnvironmentFromList(envList);
    m_stdoutFilePath = getJsStringProperty(ctx, *scriptValue, stdoutFilePathProperty());
    m_stderrFilePath = getJsStringProperty(ctx, *scriptValue, stderrFilePathProperty());

    m_predefinedProperties
            << programProperty()
            << argumentsProperty()
            << workingDirProperty()
            << maxExitCodeProperty()
            << stdoutFilterFunctionProperty()
            << stderrFilterFunctionProperty()
            << responseFileThresholdProperty()
            << responseFileArgumentIndexProperty()
            << responseFileUsagePrefixProperty()
            << environmentProperty()
            << stdoutFilePathProperty()
            << stderrFilePathProperty();
    applyCommandProperties(ctx, scriptValue);
}

QString ProcessCommand::descriptionForCancelMessage(const QString &productName) const
{
    return description() + QLatin1String(" (") + QDir::toNativeSeparators(m_program)
           + QLatin1String(") [") + productName + QLatin1Char(']');
}

QStringList ProcessCommand::relevantEnvVars() const
{
    QStringList vars = m_relevantEnvVars;
    if (!FileInfo::isAbsolute(program()))
        vars << StringConstants::pathEnvVar();
    return vars;
}

void ProcessCommand::addRelevantEnvValue(const QString &key, const QString &value)
{
    m_relevantEnvValues.insert(key, value);
}

void ProcessCommand::load(PersistentPool &pool)
{
    AbstractCommand::load(pool);
    serializationOp<PersistentPool::Load>(pool);
}

void ProcessCommand::store(PersistentPool &pool)
{
    AbstractCommand::store(pool);
    serializationOp<PersistentPool::Store>(pool);
}

static QString currentImportScopeName(JSContext *ctx)
{
    const ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
    const JSValueList &contextStack = engine->contextStack();
    for (auto it = contextStack.rbegin(); it != contextStack.rend(); ++it) {
        if (!JS_IsObject(*it))
            continue;
        const ScopedJsValue v(ctx, getJsProperty(ctx, *it,
                StringConstants::importScopeNamePropertyInternal()));
        if (JS_IsString(v))
            return getJsString(ctx, v);
    }
    return {};
}

static JSValue js_JavaScriptCommand(JSContext *ctx, JSValueConst, JSValueConst,
                                    int argc, JSValueConst *, int)
{
    if (argc > 0)
        return throwError(ctx, Tr::tr("JavaScriptCommand c'tor doesn't take arguments."));

    static JavaScriptCommandPtr commandPrototype = JavaScriptCommand::create();
    JSValue cmd = js_CommandBase(ctx);
    setJsProperty(ctx, cmd, StringConstants::classNameProperty(),
                  makeJsString(ctx, StringConstants::javaScriptCommandType()));
    setJsProperty(ctx, cmd, StringConstants::sourceCodeProperty(),
                  makeJsString(ctx, commandPrototype->sourceCode()));
    setJsProperty(ctx, cmd, StringConstants::importScopeNamePropertyInternal(),
                  makeJsString(ctx, currentImportScopeName(ctx)));
    return cmd;
}

void JavaScriptCommand::setupForJavaScript(ScriptEngine *engine, JSValue targetObject)
{
    engine->registerClass(StringConstants::javaScriptCommandType().toUtf8().constData(),
                          js_JavaScriptCommand, nullptr, targetObject);
}

JavaScriptCommand::JavaScriptCommand() = default;

bool JavaScriptCommand::equals(const AbstractCommand *otherAbstractCommand) const
{
    if (!AbstractCommand::equals(otherAbstractCommand))
        return false;
    auto const other = static_cast<const JavaScriptCommand *>(otherAbstractCommand);
    return m_sourceCode == other->m_sourceCode;
}

void JavaScriptCommand::fillFromScriptValue(JSContext *ctx, const JSValue *scriptValue,
                                            const CodeLocation &codeLocation)
{
    AbstractCommand::fillFromScriptValue(ctx, scriptValue, codeLocation);

    const ScopedJsValue importScope(ctx, getJsProperty(ctx, *scriptValue,
            StringConstants::importScopeNamePropertyInternal()));
    if (JS_IsString(importScope))
        m_scopeName = getJsString(ctx, importScope);

    const ScopedJsValue sourceCode(ctx, getJsProperty(ctx, *scriptValue,
                                                      StringConstants::sourceCodeProperty()));
    m_sourceCode = invokedSourceCode(ctx, sourceCode);

    m_predefinedProperties << StringConstants::classNameProperty()
                           << StringConstants::sourceCodeProperty()
                           << StringConstants::importScopeNamePropertyInternal();
    applyCommandProperties(ctx, scriptValue);
}

void JavaScriptCommand::load(PersistentPool &pool)
{
    AbstractCommand::load(pool);
    serializationOp<PersistentPool::Load>(pool);
}

void JavaScriptCommand::store(PersistentPool &pool)
{
    AbstractCommand::store(pool);
    serializationOp<PersistentPool::Store>(pool);
}

void CommandList::load(PersistentPool &pool)
{
    m_commands.clear();
    int count = pool.load<int>();
    m_commands.reserve(count);
    while (--count >= 0) {
        const auto cmdType = pool.load<quint8>();
        AbstractCommandPtr cmd;
        switch (cmdType) {
        case AbstractCommand::JavaScriptCommandType:
            cmd = pool.load<JavaScriptCommandPtr>();
            break;
        case AbstractCommand::ProcessCommandType:
            cmd = pool.load<ProcessCommandPtr>();
            break;
        default:
            QBS_CHECK(false);
        }
        addCommand(cmd);
    }
}

void CommandList::store(PersistentPool &pool) const
{
    pool.store(int(m_commands.size()));
    for (const AbstractCommandPtr &cmd : m_commands) {
        pool.store(static_cast<quint8>(cmd->type()));
        pool.store(cmd);
    }
}

bool operator==(const CommandList &l1, const CommandList &l2)
{
    if (l1.size() != l2.size())
        return false;
    for (int i = 0; i < l1.size(); ++i)
        if (!l1.commandAt(i)->equals(l2.commandAt(i).get()))
            return false;
    return true;
}

} // namespace Internal
} // namespace qbs
