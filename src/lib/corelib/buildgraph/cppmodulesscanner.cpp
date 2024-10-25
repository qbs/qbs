/****************************************************************************
**
** Copyright (C) 2024 Janet Black.
** Copyright (C) 2024 Ivan Komissarov (abbapoh@gmail.com).
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
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

#include "cppmodulesscanner.h"

#include "artifact.h"
#include "depscanner.h"
#include "productbuilddata.h"
#include "projectbuilddata.h"
#include "rawscanresults.h"

#include <cppscanner/cppscanner.h>
#include <language/scriptengine.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <plugins/scanner/scanner.h>
#include <tools/fileinfo.h>
#include <tools/scripttools.h>

namespace qbs::Internal {

static QString cppModulesScannerJsName()
{
    return QStringLiteral("CppModulesScanner");
}

CppModulesScanner::CppModulesScanner(ScriptEngine *engine, JSValue targetScriptValue)
    : m_engine(engine)
    , m_targetScriptValue(JS_DupValue(engine->context(), targetScriptValue))
{
    JSValue scannerObj = JS_NewObjectClass(engine->context(), engine->dataWithPtrClass());
    attachPointerTo(scannerObj, this);
    setJsProperty(engine->context(), targetScriptValue, cppModulesScannerJsName(), scannerObj);
    JSValue applyFunction = JS_NewCFunction(
        engine->context(), &js_apply, qPrintable(cppModulesScannerJsName()), 1);
    setJsProperty(engine->context(), scannerObj, QStringLiteral("apply"), applyFunction);
}

CppModulesScanner::~CppModulesScanner()
{
    setJsProperty(
        m_engine->context(), m_targetScriptValue, cppModulesScannerJsName(), JS_UNDEFINED);
    JS_FreeValue(m_engine->context(), m_targetScriptValue);
}

static RawScanResult runScannerForArtifact(const Artifact *artifact)
{
    const QString &filepath = artifact->filePath();
    RawScanResults &rawScanResults
        = artifact->product->topLevelProject()->buildData->rawScanResults;
    // TODO: use the same id for all c++ scanners?
    auto predicate = [](const PropertyMapConstPtr &, const PropertyMapConstPtr &) { return true; };
    RawScanResults::ScanData &scanData = rawScanResults.findScanData(
        artifact, cppModulesScannerJsName(), artifact->properties, predicate);
    if (scanData.lastScanTime < artifact->timestamp()) {
        FileTags tags = artifact->fileTags();
        if (tags.contains("cpp.combine")) {
            tags.remove("cpp.combine");
            tags.insert("cpp");
        }

        const QByteArray tagsForScanner = tags.toStringList().join(QLatin1Char(',')).toLatin1();
        CppScannerContext context;
        const bool ok = scanCppFile(
            context, filepath, {tagsForScanner.data(), size_t(tagsForScanner.size())}, true, true);

        if (!ok)
            return scanData.rawScanResult;

        scanData.rawScanResult.providesModule = QString::fromUtf8(context.providesModule);
        scanData.rawScanResult.isInterfaceModule = context.isInterface;
        scanData.rawScanResult.requiresModules.clear();
        for (const auto &module : context.requiresModules)
            scanData.rawScanResult.requiresModules += QString::fromUtf8(module);

        scanData.lastScanTime = FileTime::currentTime();
    }
    return scanData.rawScanResult;
}

JSValue CppModulesScanner::js_apply(JSContext *ctx, JSValue this_val, int argc, JSValue *argv)
{
    if (argc < 1)
        return throwError(ctx, Tr::tr("CppModulesScanner.apply() requires an argument."));
    ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
    const auto scanner = attachedPointer<CppModulesScanner>(this_val, engine->dataWithPtrClass());
    return scanner->apply(engine, attachedPointer<Artifact>(argv[0], engine->dataWithPtrClass()));
}

JSValue CppModulesScanner::apply(ScriptEngine *engine, const Artifact *artifact)
{
    JSContext * const ctx = m_engine->context();
    if (!artifact)
        return JSValue{};

    RawScanResult scanResult = runScannerForArtifact(artifact);

    JSValue obj = engine->newObject();
    setJsProperty(
        ctx, obj, QStringLiteral("providesModule"), makeJsString(ctx, scanResult.providesModule));
    setJsProperty(
        ctx,
        obj,
        QStringLiteral("isInterfaceModule"),
        JS_NewBool(ctx, scanResult.isInterfaceModule));
    setJsProperty(
        ctx,
        obj,
        QStringLiteral("requiresModules"),
        makeJsStringList(ctx, scanResult.requiresModules));
    engine->setUsesIo();
    return obj;
}

} // namespace qbs::Internal
