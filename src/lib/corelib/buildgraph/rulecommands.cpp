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

#include "rulecommands.h"
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <tools/persistence.h>
#include <tools/qbsassert.h>
#include <tools/stringconstants.h>

#include <QtCore/qfile.h>

#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptvalueiterator.h>

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
static QString silentProperty() { return QStringLiteral("silent"); }
static QString sourceCodeProperty() { return QStringLiteral("sourceCode"); }
static QString stderrFilePathProperty() { return QStringLiteral("stderrFilePath"); }
static QString stderrFilterFunctionProperty() { return QStringLiteral("stderrFilterFunction"); }
static QString stdoutFilePathProperty() { return QStringLiteral("stdoutFilePath"); }
static QString stdoutFilterFunctionProperty() { return QStringLiteral("stdoutFilterFunction"); }
static QString workingDirProperty() { return QStringLiteral("workingDirectory"); }

static QString invokedSourceCode(const QScriptValue codeOrFunction)
{
    const QString &code = codeOrFunction.toString();
    return codeOrFunction.isFunction() ? QStringLiteral("(") + code + QStringLiteral(")()") : code;
}

AbstractCommand::AbstractCommand()
    : m_description(defaultDescription()),
      m_extendedDescription(defaultExtendedDescription()),
      m_highlight(defaultHighLight()),
      m_ignoreDryRun(defaultIgnoreDryRun()),
      m_silent(defaultIsSilent())
{
}

AbstractCommand::~AbstractCommand()
{
}

bool AbstractCommand::equals(const AbstractCommand *other) const
{
    return type() == other->type()
            && m_description == other->m_description
            && m_extendedDescription == other->m_extendedDescription
            && m_highlight == other->m_highlight
            && m_ignoreDryRun == other->m_ignoreDryRun
            && m_silent == other->m_silent
            && m_properties == other->m_properties;
}

void AbstractCommand::fillFromScriptValue(const QScriptValue *scriptValue, const CodeLocation &codeLocation)
{
    m_description = scriptValue->property(StringConstants::descriptionProperty()).toString();
    m_extendedDescription = scriptValue->property(extendedDescriptionProperty()).toString();
    m_highlight = scriptValue->property(highlightProperty()).toString();
    m_ignoreDryRun = scriptValue->property(ignoreDryRunProperty()).toBool();
    m_silent = scriptValue->property(silentProperty()).toBool();
    m_codeLocation = codeLocation;

    m_predefinedProperties
            << StringConstants::descriptionProperty()
            << extendedDescriptionProperty()
            << highlightProperty()
            << ignoreDryRunProperty()
            << silentProperty();
}

void AbstractCommand::load(PersistentPool &pool)
{
    pool.load(m_description);
    pool.load(m_extendedDescription);
    pool.load(m_highlight);
    pool.load(m_ignoreDryRun);
    pool.load(m_silent);
    pool.load(m_codeLocation);
    pool.load(m_properties);
}

void AbstractCommand::store(PersistentPool &pool) const
{
    pool.store(m_description);
    pool.store(m_extendedDescription);
    pool.store(m_highlight);
    pool.store(m_ignoreDryRun);
    pool.store(m_silent);
    pool.store(m_codeLocation);
    pool.store(m_properties);
}

void AbstractCommand::applyCommandProperties(const QScriptValue *scriptValue)
{
    QScriptValueIterator it(*scriptValue);
    while (it.hasNext()) {
        it.next();
        if (m_predefinedProperties.contains(it.name()))
            continue;
        const QVariant value = it.value().toVariant();
        if (QMetaType::Type(value.type()) == QMetaType::QObjectStar) {
            throw ErrorInfo(Tr::tr("Property '%1' has a type unsuitable for storing in a command "
                                   "object.").arg(it.name()), m_codeLocation);
        }
        m_properties.insert(it.name(), value);
    }
}

static QScriptValue js_CommandBase(QScriptContext *context, QScriptEngine *engine)
{
    QScriptValue cmd = context->thisObject();
    QBS_ASSERT(context->isCalledAsConstructor(), cmd = engine->newObject());
    cmd.setProperty(StringConstants::descriptionProperty(),
                    engine->toScriptValue(AbstractCommand::defaultDescription()));
    cmd.setProperty(extendedDescriptionProperty(),
                    engine->toScriptValue(AbstractCommand::defaultExtendedDescription()));
    cmd.setProperty(highlightProperty(),
                    engine->toScriptValue(AbstractCommand::defaultHighLight()));
    cmd.setProperty(ignoreDryRunProperty(),
                    engine->toScriptValue(AbstractCommand::defaultIgnoreDryRun()));
    cmd.setProperty(silentProperty(),
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
    cmd.setProperty(StringConstants::classNameProperty(),
                    engine->toScriptValue(StringConstants::commandType()));
    cmd.setProperty(programProperty(), program);
    cmd.setProperty(argumentsProperty(), arguments);
    cmd.setProperty(workingDirProperty(),
                    engine->toScriptValue(commandPrototype->workingDir()));
    cmd.setProperty(maxExitCodeProperty(),
                    engine->toScriptValue(commandPrototype->maxExitCode()));
    cmd.setProperty(stdoutFilterFunctionProperty(),
                    engine->toScriptValue(commandPrototype->stdoutFilterFunction()));
    cmd.setProperty(stderrFilterFunctionProperty(),
                    engine->toScriptValue(commandPrototype->stderrFilterFunction()));
    cmd.setProperty(responseFileThresholdProperty(),
                    engine->toScriptValue(commandPrototype->responseFileThreshold()));
    cmd.setProperty(responseFileArgumentIndexProperty(),
                    engine->toScriptValue(commandPrototype->responseFileArgumentIndex()));
    cmd.setProperty(responseFileUsagePrefixProperty(),
                    engine->toScriptValue(commandPrototype->responseFileUsagePrefix()));
    cmd.setProperty(stdoutFilePathProperty(),
                    engine->toScriptValue(commandPrototype->stdoutFilePath()));
    cmd.setProperty(stderrFilePathProperty(),
                    engine->toScriptValue(commandPrototype->stderrFilePath()));
    cmd.setProperty(environmentProperty(),
                    engine->toScriptValue(commandPrototype->environment().toStringList()));
    cmd.setProperty(ignoreDryRunProperty(),
                    engine->toScriptValue(commandPrototype->ignoreDryRun()));
    return cmd;
}


void ProcessCommand::setupForJavaScript(QScriptValue targetObject)
{
    QBS_CHECK(targetObject.isObject());
    QScriptValue ctor = targetObject.engine()->newFunction(js_Command, 2);
    targetObject.setProperty(StringConstants::commandType(), ctor);
}

ProcessCommand::ProcessCommand()
    : m_maxExitCode(0)
    , m_responseFileThreshold(defaultResponseFileThreshold())
    , m_responseFileArgumentIndex(0)
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
            && m_stdoutFilePath == other->m_stdoutFilePath
            && m_stderrFilePath == other->m_stderrFilePath
            && m_relevantEnvVars == other->m_relevantEnvVars
            && m_environment == other->m_environment;
}

void ProcessCommand::fillFromScriptValue(const QScriptValue *scriptValue, const CodeLocation &codeLocation)
{
    AbstractCommand::fillFromScriptValue(scriptValue, codeLocation);
    m_program = scriptValue->property(programProperty()).toString();
    m_arguments = scriptValue->property(argumentsProperty()).toVariant().toStringList();
    m_workingDir = scriptValue->property(workingDirProperty()).toString();
    m_maxExitCode = scriptValue->property(maxExitCodeProperty()).toInt32();

    // toString() is required, presumably due to QtScript bug that manifests itself on Windows
    const QScriptValue stdoutFilterFunction
            = scriptValue->property(stdoutFilterFunctionProperty()).toString();

    m_stdoutFilterFunction = invokedSourceCode(stdoutFilterFunction);

    // toString() is required, presumably due to QtScript bug that manifests itself on Windows
    const QScriptValue stderrFilterFunction
            = scriptValue->property(stderrFilterFunctionProperty()).toString();

    m_stderrFilterFunction = invokedSourceCode(stderrFilterFunction);
    m_relevantEnvVars = scriptValue->property(QStringLiteral("relevantEnvironmentVariables"))
            .toVariant().toStringList();
    m_responseFileThreshold = scriptValue->property(responseFileThresholdProperty())
            .toInt32();
    m_responseFileArgumentIndex = scriptValue->property(responseFileArgumentIndexProperty())
            .toInt32();
    m_responseFileUsagePrefix = scriptValue->property(responseFileUsagePrefixProperty())
            .toString();
    QStringList envList = scriptValue->property(environmentProperty()).toVariant()
            .toStringList();
    getEnvironmentFromList(envList);
    m_stdoutFilePath = scriptValue->property(stdoutFilePathProperty()).toString();
    m_stderrFilePath = scriptValue->property(stderrFilePathProperty()).toString();

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
    applyCommandProperties(scriptValue);
}

QStringList ProcessCommand::relevantEnvVars() const
{
    QStringList vars = m_relevantEnvVars;
    if (!FileInfo::isAbsolute(program()))
        vars << StringConstants::pathEnvVar();
    return vars;
}

void ProcessCommand::load(PersistentPool &pool)
{
    AbstractCommand::load(pool);
    pool.load(m_program);
    pool.load(m_arguments);
    pool.load(m_environment);
    pool.load(m_workingDir);
    pool.load(m_stdoutFilterFunction);
    pool.load(m_stderrFilterFunction);
    pool.load(m_responseFileUsagePrefix);
    pool.load(m_maxExitCode);
    pool.load(m_responseFileThreshold);
    pool.load(m_responseFileArgumentIndex);
    pool.load(m_relevantEnvVars);
    pool.load(m_stdoutFilePath);
    pool.load(m_stderrFilePath);
}

void ProcessCommand::store(PersistentPool &pool) const
{
    AbstractCommand::store(pool);
    pool.store(m_program);
    pool.store(m_arguments);
    pool.store(m_environment);
    pool.store(m_workingDir);
    pool.store(m_stdoutFilterFunction);
    pool.store(m_stderrFilterFunction);
    pool.store(m_responseFileUsagePrefix);
    pool.store(m_maxExitCode);
    pool.store(m_responseFileThreshold);
    pool.store(m_responseFileArgumentIndex);
    pool.store(m_relevantEnvVars);
    pool.store(m_stdoutFilePath);
    pool.store(m_stderrFilePath);
}

static QString currentImportScopeName(QScriptContext *context)
{
    for (; context; context = context->parentContext()) {
        QScriptValue v = context->thisObject()
                .property(StringConstants::importScopeNamePropertyInternal());
        if (v.isString())
            return v.toString();
    }
    return QString();
}

static QScriptValue js_JavaScriptCommand(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(!context->isCalledAsConstructor()))
        return context->throwError(Tr::tr("JavaScriptCommand constructor called without new."));
    if (Q_UNLIKELY(context->argumentCount() != 0)) {
        return context->throwError(QScriptContext::SyntaxError,
                                  Tr::tr("JavaScriptCommand c'tor doesn't take arguments."));
    }

    static JavaScriptCommandPtr commandPrototype = JavaScriptCommand::create();
    QScriptValue cmd = js_CommandBase(context, engine);
    cmd.setProperty(StringConstants::classNameProperty(),
                    engine->toScriptValue(StringConstants::javaScriptCommandType()));
    cmd.setProperty(sourceCodeProperty(),
                    engine->toScriptValue(commandPrototype->sourceCode()));
    cmd.setProperty(StringConstants::importScopeNamePropertyInternal(),
                    engine->toScriptValue(currentImportScopeName(context)));

    return cmd;
}

void JavaScriptCommand::setupForJavaScript(QScriptValue targetObject)
{
    QBS_CHECK(targetObject.isObject());
    QScriptValue ctor = targetObject.engine()->newFunction(js_JavaScriptCommand, 0);
    targetObject.setProperty(StringConstants::javaScriptCommandType(), ctor);
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

    const QScriptValue importScope = scriptValue->property(
                StringConstants::importScopeNamePropertyInternal());
    if (importScope.isString())
        m_scopeName = importScope.toString();

    const QScriptValue sourceCode = scriptValue->property(sourceCodeProperty());
    m_sourceCode = invokedSourceCode(sourceCode);

    m_predefinedProperties << StringConstants::classNameProperty() << sourceCodeProperty()
                           << StringConstants::importScopeNamePropertyInternal();
    applyCommandProperties(scriptValue);
}

void JavaScriptCommand::load(PersistentPool &pool)
{
    AbstractCommand::load(pool);
    pool.load(m_scopeName);
    pool.load(m_sourceCode);
}

void JavaScriptCommand::store(PersistentPool &pool) const
{
    AbstractCommand::store(pool);
    pool.store(m_scopeName);
    pool.store(m_sourceCode);
}

QList<AbstractCommandPtr> loadCommandList(PersistentPool &pool)
{
    QList<AbstractCommandPtr> commands;
    int count = pool.load<int>();
    commands.reserve(count);
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
        commands.push_back(cmd);
    }
    return commands;
}

void storeCommandList(const QList<AbstractCommandPtr> &commands, PersistentPool &pool)
{
    pool.store(commands.size());
    for (const AbstractCommandPtr &cmd : commands) {
        pool.store(static_cast<quint8>(cmd->type()));
        pool.store(cmd);
    }
}

bool commandListsAreEqual(const QList<AbstractCommandPtr> &l1, const QList<AbstractCommandPtr> &l2)
{
    if (l1.size() != l2.size())
        return false;
    for (int i = 0; i < l1.size(); ++i)
        if (!l1.at(i)->equals(l2.at(i).get()))
            return false;
    return true;
}

} // namespace Internal
} // namespace qbs
