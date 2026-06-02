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

#include "depscanner.h"
#include "artifact.h"
#include "buildgraph.h"
#include "projectbuilddata.h"
#include "transformer.h"

#include <jsextensions/moduleproperties.h>
#include <language/language.h>
#include <language/propertymapinternal.h>
#include <language/resolvedfilecontext.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <plugins/scanner/scanner.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/scripttools.h>
#include <tools/stringconstants.h>

#include <QtCore/qvariant.h>

#include <vector>

namespace qbs {
namespace Internal {

static QStringList collectProductBuildDirectories(const ResolvedProduct *product)
{
    QStringList buildDirectories;
    if (!product)
        return buildDirectories;

    buildDirectories << product->buildDirectory();
    // Add build directories of dependent products
    for (const auto &dep : product->dependencies) {
        if (dep.product)
            buildDirectories << dep.product->buildDirectory();
    }
    return buildDirectories;
}

DependencyScanner::DependencyScanner(
    ResolvedScannerPtr scanner, ScriptEngine *engine, ScannerPlugin *plugin)
    : m_scanner(std::move(scanner))
    , m_engine(engine)
    , m_global(engine->context(), JS_NewObjectProto(engine->context(), m_engine->globalObject()))
    , m_plugin(plugin)
{
    if (!m_plugin) {
        setupScriptEngineForFile(
            m_engine, m_scanner->scanScript.fileContext(), m_global, ObserveMode::Enabled);
    }
}

QString DependencyScanner::id() const
{
    if (m_id.isEmpty())
        m_id = createId();
    return m_id;
}

QStringList DependencyScanner::collectSearchPaths(Artifact *artifact)
{
    if (!m_plugin) {
        if (!m_scanner->searchPathsScript.isValid())
            return {};
        return evaluateStringListScript(
            artifact,
            nullptr,
            m_scanner->searchPathsScript,
            Tr::tr("Search paths script must return an array of paths."));
    }

    const QStringList buildDirectories = collectProductBuildDirectories(artifact->product.get());
    return m_plugin->collectSearchPaths(
        artifact->properties->value(), buildDirectories, artifact->fileTags().toStringList());
}

DependencyScanner::ScanResult DependencyScanner::collectScanResult(
    Artifact *artifact, FileResourceBase *file, const char *fileTags)
{
    ScanResult result;
    if (m_plugin) {
        const ScannerScanResult scanResult = m_plugin->scan(
            file->filePath(), fileTags, artifact->properties->value());
        result.dependencies = scanResult.dependencies;
        result.scannerProperties = scanResult.scannerProperties;
    } else {
        result = evaluateScanScript(artifact, file, m_scanner->scanScript);
    }
    result.dependencies.removeDuplicates();
    return result;
}

bool DependencyScanner::recursive() const
{
    return m_scanner->recursive;
}

QString DependencyScanner::createId() const
{
    return m_scanner->scannerId;
}

bool DependencyScanner::areModulePropertiesCompatible(
    const PropertyMapConstPtr &m1, const PropertyMapConstPtr &m2) const
{
    // This changes when our C++ scanner starts taking defines into account.
    if (m_plugin)
        return true;
    // TODO: This should probably be made more fine-grained. Perhaps the Scanner item
    //       could declare the relevant properties, or we could figure them out automatically
    //       somehow.
    return m1 == m2 || *m1 == *m2;
}

bool DependencyScanner::cacheIsPerFile() const
{
    return m_scanner->cacheIsPerFile;
}

class ScriptEngineActiveFlagGuard
{
    ScriptEngine *m_engine;
public:
    ScriptEngineActiveFlagGuard(ScriptEngine *engine)
        : m_engine(engine)
    {
        m_engine->setActive(true);
    }

    ~ScriptEngineActiveFlagGuard()
    {
        m_engine->setActive(false);
    }
};

static QStringList jsValueToStringList(JSContext *ctx, JSValue value)
{
    QStringList list;
    if (!JS_IsArray(value))
        return list;
    const int count = getJsIntProperty(ctx, value, StringConstants::lengthProperty());
    list.reserve(count);
    for (qint32 i = 0; i < count; ++i) {
        JSValue item = JS_GetPropertyUint32(ctx, value, i);
        if (!JS_IsUninitialized(item) && !JS_IsUndefined(item))
            list.push_back(getJsString(ctx, item));
        JS_FreeValue(ctx, item);
    }
    return list;
}

JSValue DependencyScanner::callScannerScript(
    Artifact *artifact,
    const FileResourceBase *fileToScan,
    const PrivateScriptFunction &script,
    const QString &errorMessagePrefix)
{
    ScriptEngineActiveFlagGuard guard(m_engine);

    if (artifact->product.get() != m_product) {
        m_product = artifact->product.get();
        setupScriptEngineForProduct(
            m_engine, artifact->product.get(), m_scanner->module.get(), m_global, true);
    }

    JSValueList args;
    args.reserve(fileToScan ? 4 : 3);
    args.push_back(getJsProperty(m_engine->context(), m_global, StringConstants::projectVar()));
    args.push_back(getJsProperty(m_engine->context(), m_global, StringConstants::productVar()));
    args.push_back(Transformer::translateFileConfig(m_engine, artifact, m_scanner->module->name));
    if (fileToScan)
        args.push_back(makeJsString(m_engine->context(), fileToScan->filePath()));
    const ScopedJsValueList argsMgr(m_engine->context(), args);

    const TemporaryGlobalObjectSetter gos(m_engine, m_global);
    const JSValue function = script.getFunction(m_engine, Tr::tr("Invalid scanner script."));
    const JSValue result = JS_Call(
        m_engine->context(), function, m_engine->globalObject(), int(args.size()), args.data());

    m_engine->mergeAndClearTrackedScriptAccesses(*m_scanner->scriptAccesses);

    if (m_engine->checkForJsError(script.location())) {
        ErrorInfo err = m_engine->getAndClearJsError();
        err.prepend(errorMessagePrefix);
        throw err;
    }

    return result;
}

QStringList DependencyScanner::evaluateStringListScript(
    Artifact *artifact,
    const FileResourceBase *fileToScan,
    const PrivateScriptFunction &script,
    const QString &invalidReturnMessage)
{
    const ScopedJsValue result(
        m_engine->context(),
        callScannerScript(
            artifact, fileToScan, script, Tr::tr("Error evaluating search paths script")));
    if (!JS_IsArray(result))
        throw ErrorInfo(invalidReturnMessage, script.location());
    return jsValueToStringList(m_engine->context(), result);
}

DependencyScanner::ScanResult DependencyScanner::evaluateScanScript(
    Artifact *artifact, const FileResourceBase *fileToScan, const PrivateScriptFunction &script)
{
    const ScopedJsValue result(
        m_engine->context(),
        callScannerScript(artifact, fileToScan, script, Tr::tr("Error evaluating scan script")));

    ScanResult scanResult;
    JSContext * const ctx = m_engine->context();
    if (JS_IsArray(result)) {
        scanResult.dependencies = jsValueToStringList(ctx, result);
        return scanResult;
    }

    if (!JS_IsObject(result) || JS_IsArray(result) || JS_IsError(result) || JS_IsRegExp(result)) {
        throw ErrorInfo(
            Tr::tr("Scan script must return an array of dependency paths or an object"),
            script.location());
    }

    const ScopedJsValue searchPathsValue(
        ctx, getJsProperty(ctx, result, StringConstants::searchPathsProperty()));
    if (!JS_IsUndefined(searchPathsValue)) {
        throw ErrorInfo(
            Tr::tr("Scan script must not return searchPaths; use the Scanner.searchPaths "
                   "script instead."),
            script.location());
    }

    const ScopedJsValue dependenciesValue(
        ctx, getJsProperty(ctx, result, StringConstants::dependenciesProperty()));
    if (!JS_IsUndefined(dependenciesValue))
        scanResult.dependencies = jsValueToStringList(ctx, dependenciesValue);

    const ScopedJsValue scannerPropertiesValue(
        ctx, getJsProperty(ctx, result, StringConstants::scannerPropertiesProperty()));
    if (!JS_IsUndefined(scannerPropertiesValue))
        scanResult.scannerProperties = getJsVariant(ctx, scannerPropertiesValue).toMap();

    return scanResult;
}

} // namespace Internal
} // namespace qbs
