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
#include "projectbuilddata.h"
#include "buildgraph.h"
#include "transformer.h"

#include <tools/error.h>
#include <logging/translator.h>
#include <language/language.h>
#include <language/propertymapinternal.h>
#include <language/resolvedfilecontext.h>
#include <language/scriptengine.h>
#include <jsextensions/moduleproperties.h>
#include <plugins/scanner/scanner.h>
#include <tools/fileinfo.h>
#include <tools/stringconstants.h>

#include <QtCore/qvariant.h>

#include <vector>

namespace qbs {
namespace Internal {

QString DependencyScanner::id() const
{
    if (m_id.isEmpty())
        m_id = createId();
    return m_id;
}

static QStringList collectCppIncludePaths(const QVariantMap &modules)
{
    QStringList result;
    const QVariantMap cpp = modules.value(StringConstants::cppModule()).toMap();
    if (cpp.empty())
        return result;

    result << cpp.value(QStringLiteral("includePaths")).toStringList();
    const bool useSystemHeaders
            = cpp.value(QStringLiteral("treatSystemHeadersAsDependencies")).toBool();
    if (useSystemHeaders) {
        result
            << cpp.value(QStringLiteral("systemIncludePaths")).toStringList()
            << cpp.value(QStringLiteral("distributionIncludePaths")).toStringList()
            << cpp.value(QStringLiteral("compilerIncludePaths")).toStringList();
    }
    result.removeDuplicates();
    return result;
}

PluginDependencyScanner::PluginDependencyScanner(ScannerPlugin *plugin)
    : m_plugin(plugin)
{
}

QStringList PluginDependencyScanner::collectSearchPaths(Artifact *artifact)
{
    if (m_plugin->flags & ScannerUsesCppIncludePaths)
        return collectCppIncludePaths(artifact->properties->value());
    return {};
}

QStringList PluginDependencyScanner::collectDependencies(Artifact *artifact, FileResourceBase *file,
                                                         const char *fileTags)
{
    Q_UNUSED(artifact);
    Set<QString> result;
    QString baseDirOfInFilePath = file->dirPath();
    const QString &filepath = file->filePath();
    void *scannerHandle = m_plugin->open(filepath.utf16(), fileTags, ScanForDependenciesFlag);
    if (!scannerHandle)
        return {};
    for (;;) {
        int flags = 0;
        int length = 0;
        const char *szOutFilePath = m_plugin->next(scannerHandle, &length, &flags);
        if (szOutFilePath == nullptr)
            break;
        QString outFilePath = QString::fromLocal8Bit(szOutFilePath, length);
        if (outFilePath.isEmpty())
            continue;
        if (flags & SC_LOCAL_INCLUDE_FLAG) {
            QString localFilePath = FileInfo::resolvePath(baseDirOfInFilePath, outFilePath);
            if (FileInfo::exists(localFilePath))
                outFilePath = localFilePath;
        }
        result += outFilePath;
    }
    m_plugin->close(scannerHandle);
    return rangeTo<QStringList>(result);
}

bool PluginDependencyScanner::recursive() const
{
    return m_plugin->flags & ScannerRecursiveDependencies;
}

const void *PluginDependencyScanner::key() const
{
    return m_plugin;
}

QString PluginDependencyScanner::createId() const
{
    return QString::fromLatin1(m_plugin->name);
}

bool PluginDependencyScanner::areModulePropertiesCompatible(const PropertyMapConstPtr &m1,
                                                            const PropertyMapConstPtr &m2) const
{
    // This changes when our C++ scanner starts taking defines into account.
    Q_UNUSED(m1);
    Q_UNUSED(m2);
    return true;
}

UserDependencyScanner::UserDependencyScanner(ResolvedScannerConstPtr scanner,
                                             ScriptEngine *engine)
    : m_scanner(std::move(scanner)),
      m_engine(engine),
      m_global(engine->context(), JS_NewObjectProto(engine->context(), m_engine->globalObject())),
      m_product(nullptr)
{
    setupScriptEngineForFile(m_engine, m_scanner->scanScript.fileContext(), m_global,
                             ObserveMode::Disabled); // TODO: QBS-1092
}

QStringList UserDependencyScanner::collectSearchPaths(Artifact *artifact)
{
    return evaluate(artifact, nullptr, m_scanner->searchPathsScript);
}

QStringList UserDependencyScanner::collectDependencies(Artifact *artifact, FileResourceBase *file,
                                                       const char *fileTags)
{
    Q_UNUSED(fileTags);
    return evaluate(artifact, file, m_scanner->scanScript);
}

bool UserDependencyScanner::recursive() const
{
    return m_scanner->recursive;
}

const void *UserDependencyScanner::key() const
{
    return m_scanner.get();
}

QString UserDependencyScanner::createId() const
{
    return m_scanner->scanScript.sourceCode();
}

bool UserDependencyScanner::areModulePropertiesCompatible(const PropertyMapConstPtr &m1,
                                                          const PropertyMapConstPtr &m2) const
{
    // TODO: This should probably be made more fine-grained. Perhaps the Scanner item
    //       could declare the relevant properties, or we could figure them out automatically
    //       somehow.
    return m1 == m2 || *m1 == *m2;
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

QStringList UserDependencyScanner::evaluate(Artifact *artifact,
        const FileResourceBase *fileToScan, const PrivateScriptFunction &script)
{
    ScriptEngineActiveFlagGuard guard(m_engine);

    if (artifact->product.get() != m_product) {
        m_product = artifact->product.get();
        setupScriptEngineForProduct(m_engine, artifact->product.get(),
                                    m_scanner->module.get(), m_global, true);
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
    const JSValue function = script.getFunction(m_engine, Tr::tr("Invalid scan script."));
    const ScopedJsValue result(
                m_engine->context(),
                JS_Call(m_engine->context(), function, m_engine->globalObject(),
                int(args.size()), args.data()));
    m_engine->clearRequestedProperties();
    if (JsException ex = m_engine->checkAndClearException(script.location())) {
        ErrorInfo err = ex.toErrorInfo();
        err.prepend(Tr::tr("Error evaluating scan script"));
        throw err;
    }
    QStringList list;
    if (JS_IsArray(m_engine->context(), result)) {
        const int count = getJsIntProperty(m_engine->context(), result,
                                           StringConstants::lengthProperty());
        list.reserve(count);
        for (qint32 i = 0; i < count; ++i) {
            JSValue item = JS_GetPropertyUint32(m_engine->context(), result, i);
            if (!JS_IsUninitialized(item) && !JS_IsUndefined(item))
                list.push_back(getJsString(m_engine->context(), item));
            JS_FreeValue(m_engine->context(), item);
        }
    }
    return list;
}

} // namespace Internal
} // namespace qbs
