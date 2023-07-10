/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2022 Raphaël Cotty <raphael.cotty@gmail.com>
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

#include "probesresolver.h"

#include "itemreader.h"
#include "loaderutils.h"

#include <api/languageinfo.h>
#include <language/evaluator.h>
#include <language/filecontext.h>
#include <language/item.h>
#include <language/language.h>
#include <language/scriptengine.h>
#include <language/value.h>
#include <logging/categories.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/profiling.h>
#include <tools/scripttools.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>

#include <quickjs.h>

namespace qbs {
namespace Internal {

static QString probeGlobalId(Item *probe)
{
    QString id;

    for (Item *obj = probe; obj; obj = obj->prototype()) {
        if (!obj->id().isEmpty()) {
            id = obj->id();
            break;
        }
    }

    if (id.isEmpty())
        return {};

    QBS_CHECK(probe->file());
    return id + QLatin1Char('_') + probe->file()->filePath();
}

ProbesResolver::ProbesResolver(LoaderState &loaderState) : m_loaderState(loaderState) {}

void ProbesResolver::resolveProbes(ProductContext &productContext, Item *item)
{
    AccumulatingTimer probesTimer(m_loaderState.parameters().logElapsedTime()
                                  ? &productContext.timingData.probes : nullptr);

    EvalContextSwitcher evalContextSwitcher(m_loaderState.evaluator().engine(),
                                            EvalContext::ProbeExecution);
    for (Item * const child : item->children()) {
        if (child->type() == ItemType::Probe)
            resolveProbe(productContext, item, child);
    }
}

void ProbesResolver::resolveProbe(ProductContext &productContext, Item *parent,
                                           Item *probe)
{
    qCDebug(lcModuleLoader) << "Resolving Probe at " << probe->location().toString();
    const QString &probeId = probeGlobalId(probe);
    if (Q_UNLIKELY(probeId.isEmpty()))
        throw ErrorInfo(Tr::tr("Probe.id must be set."), probe->location());
    const bool isProjectLevelProbe
        = parent->type() == ItemType::Project
          || productContext.name.startsWith(StringConstants::shadowProductPrefix());
    const JSSourceValueConstPtr configureScript
            = probe->sourceProperty(StringConstants::configureProperty());
    QBS_CHECK(configureScript);
    if (Q_UNLIKELY(configureScript->sourceCode() == StringConstants::undefinedValue()))
        throw ErrorInfo(Tr::tr("Probe.configure must be set."), probe->location());
    using ProbeProperty = std::pair<QString, ScopedJsValue>;
    std::vector<ProbeProperty> probeBindings;
    Evaluator &evaluator = m_loaderState.evaluator();
    ScriptEngine * const engine = evaluator.engine();
    JSContext * const ctx = engine->context();
    QVariantMap initialProperties;
    for (Item *obj = probe; obj; obj = obj->prototype()) {
        const Item::PropertyMap &props = obj->properties();
        for (auto it = props.cbegin(); it != props.cend(); ++it) {
            const QString &name = it.key();
            if (name == StringConstants::configureProperty())
                continue;
            const JSValue value = evaluator.value(probe, name);
            probeBindings.emplace_back(name, ScopedJsValue(ctx, value));
            if (name != StringConstants::conditionProperty())
                initialProperties.insert(name, getJsVariant(ctx, value));
        }
    }
    const bool condition = evaluator.boolValue(probe, StringConstants::conditionProperty());
    const QString &sourceCode = configureScript->sourceCode().toString();
    ProbeConstPtr resolvedProbe;
    std::lock_guard lock(m_loaderState.topLevelProject().probesCacheLock());
    m_loaderState.topLevelProject().incrementProbesCount();
    if (isProjectLevelProbe) {
        resolvedProbe = findOldProjectProbe(probeId, condition, initialProperties, sourceCode);
    } else {
        resolvedProbe = findOldProductProbe(productContext.uniqueName(), condition,
                                            initialProperties, sourceCode);
    }
    if (!resolvedProbe) {
        resolvedProbe = findCurrentProbe(probe->location(), condition, initialProperties);
        if (resolvedProbe) {
            qCDebug(lcModuleLoader) << "probe results cached from current run";
            m_loaderState.topLevelProject().incrementReusedCurrentProbesCount();
        }
    } else {
        qCDebug(lcModuleLoader) << "probe results cached from earlier run";
        m_loaderState.topLevelProject().incrementReusedOldProbesCount();
    }
    ScopedJsValue configureScope(ctx, JS_UNDEFINED);
    std::vector<QString> importedFilesUsedInConfigure;
    if (!condition) {
        qCDebug(lcModuleLoader) << "Probe disabled; skipping";
    } else if (!resolvedProbe) {
        m_loaderState.topLevelProject().incrementRunProbesCount();
        qCDebug(lcModuleLoader) << "configure script needs to run";
        const Evaluator::FileContextScopes fileCtxScopes
                = evaluator.fileContextScopes(configureScript->file());
        configureScope.setValue(engine->newObject());
        for (const ProbeProperty &b : probeBindings)
            setJsProperty(ctx, configureScope, b.first, JS_DupValue(ctx, b.second));
        engine->clearRequestedProperties();
        ScopedJsValue sv(ctx, engine->evaluate(JsValueOwner::Caller,
                configureScript->sourceCodeForEvaluation(), {}, 1,
                {fileCtxScopes.fileScope, fileCtxScopes.importScope, configureScope}));
        if (JsException ex = engine->checkAndClearException(configureScript->location()))
            throw ex.toErrorInfo();
        importedFilesUsedInConfigure = engine->importedFilesUsedInScript();
    } else {
        importedFilesUsedInConfigure = resolvedProbe->importedFilesUsed();
    }
    QVariantMap properties;
    VariantValuePtr storedValue;
    QMap<QString, VariantValuePtr> storedValues;
    for (const ProbeProperty &b : probeBindings) {
        QVariant newValue;
        if (resolvedProbe) {
            newValue = resolvedProbe->properties().value(b.first);
        } else {
            if (condition) {
                JSValue v = getJsProperty(ctx, configureScope, b.first);
                const JSValue saved = v;
                ScopedJsValue valueMgr(ctx, saved);
                const PropertyDeclaration decl = probe->propertyDeclaration(b.first);
                evaluator.convertToPropertyType(decl, probe->location(), v);

                // If the value was converted from scalar to array as per our convenience
                // functionality, then the original value is now the only element of a
                // newly allocated array and thus gets deleted via that array.
                // The array itself is owned by the script engine, so we must stay out of
                // memory management here.
                if (v != saved)
                    valueMgr.setValue(JS_UNDEFINED);

                if (JsException ex = engine->checkAndClearException({}))
                    throw ex.toErrorInfo();
                newValue = getJsVariant(ctx, v);
                // special case, string lists are represented as js arrays and and we don't type
                // info when converting
                if (decl.type() == PropertyDeclaration::StringList
                    && newValue.userType() == QMetaType::QVariantList) {
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
                    newValue.convert(QMetaType(QMetaType::QStringList));
#else
                    newValue.convert(QMetaType::QStringList);
#endif
                }
            } else {
                newValue = initialProperties.value(b.first);
            }
        }
        if (newValue != getJsVariant(ctx, b.second)) {
            if (!resolvedProbe)
                storedValue = VariantValue::createStored(newValue);
            else
                storedValue = resolvedProbe->values().value(b.first);

            probe->setProperty(b.first, storedValue);
        }
        if (!resolvedProbe) {
            properties.insert(b.first, newValue);
            storedValues[b.first] = storedValue;
        }
    }
    if (!resolvedProbe) {
        resolvedProbe = Probe::create(probeId, probe->location(), condition,
                                      sourceCode, properties, initialProperties, storedValues,
                                      importedFilesUsedInConfigure);
        m_loaderState.topLevelProject().addNewlyResolvedProbe(resolvedProbe);
    }
    if (isProjectLevelProbe)
        m_loaderState.topLevelProject().addProjectLevelProbe(resolvedProbe);
    else
        productContext.probes << resolvedProbe;
}

ProbeConstPtr ProbesResolver::findOldProjectProbe(
        const QString &globalId,
        bool condition,
        const QVariantMap &initialProperties,
        const QString &sourceCode) const
{
    if (m_loaderState.parameters().forceProbeExecution())
        return {};
    return m_loaderState.topLevelProject().findOldProjectProbe(globalId,
                                                               [&](const ProbeConstPtr &oldProbe) {
        return probeMatches(oldProbe, condition, initialProperties, sourceCode, CompareScript::Yes);
    });
}

ProbeConstPtr ProbesResolver::findOldProductProbe(
        const QString &productName,
        bool condition,
        const QVariantMap &initialProperties,
        const QString &sourceCode) const
{
    if (m_loaderState.parameters().forceProbeExecution())
        return {};
    return m_loaderState.topLevelProject().findOldProductProbe(productName,
                                                               [&](const ProbeConstPtr &oldProbe) {
        return probeMatches(oldProbe, condition, initialProperties, sourceCode, CompareScript::Yes);
    });
}

ProbeConstPtr ProbesResolver::findCurrentProbe(
        const CodeLocation &location,
        bool condition,
        const QVariantMap &initialProperties) const
{
    return m_loaderState.topLevelProject().findCurrentProbe(location,
                                                            [&](const ProbeConstPtr &probe) {
        return probeMatches(probe, condition, initialProperties, QString(), CompareScript::No);
    });
}

bool ProbesResolver::probeMatches(const ProbeConstPtr &probe, bool condition,
        const QVariantMap &initialProperties, const QString &configureScript,
        CompareScript compareScript) const
{
    return probe->condition() == condition
            && probe->initialProperties() == initialProperties
            && (compareScript == CompareScript::No
                || (probe->configureScript() == configureScript
                    && !probe->needsReconfigure(m_loaderState.topLevelProject().lastResolveTime())));
}

} // namespace Internal
} // namespace qbs
