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
#include <language/scriptengine.h>
#include <jsextensions/moduleproperties.h>
#include <plugins/scanner/scanner.h>
#include <tools/fileinfo.h>

#include <QtCore/qvariant.h>

#include <QtScript/qscriptcontext.h>

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
    const QVariantMap cpp = modules.value(QLatin1String("cpp")).toMap();
    if (cpp.isEmpty())
        return result;

    result << cpp.value(QLatin1String("includePaths")).toStringList();
    const bool useSystemHeaders
            = cpp.value(QLatin1String("treatSystemHeadersAsDependencies")).toBool();
    if (useSystemHeaders) {
        result
            << cpp.value(QLatin1String("systemIncludePaths")).toStringList()
            << cpp.value(QLatin1String("distributionIncludePaths")).toStringList()
            << cpp.value(QLatin1String("compilerIncludePaths")).toStringList();
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
    if (m_plugin->flags & ScannerUsesCppIncludePaths) {
        QVariantMap modules = artifact->properties->value().value(QLatin1String("modules")).toMap();
        return collectCppIncludePaths(modules);
    } else {
        return QStringList();
    }
}

QStringList PluginDependencyScanner::collectDependencies(FileResourceBase *file,
                                                         const char *fileTags)
{
    Set<QString> result;
    QString baseDirOfInFilePath = file->dirPath();
    const QString &filepath = file->filePath();
    void *scannerHandle = m_plugin->open(filepath.utf16(), fileTags, ScanForDependenciesFlag);
    if (!scannerHandle)
        return QStringList();
    forever {
        int flags = 0;
        int length = 0;
        const char *szOutFilePath = m_plugin->next(scannerHandle, &length, &flags);
        if (szOutFilePath == 0)
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
    return QStringList(result.toList());
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

UserDependencyScanner::UserDependencyScanner(const ResolvedScannerConstPtr &scanner,
        const Logger &logger, ScriptEngine *engine)
    : m_scanner(scanner),
      m_logger(logger),
      m_engine(engine),
      m_observer(m_engine),
      m_product(0)
{
    m_global = m_engine->newObject();
    m_global.setPrototype(m_engine->globalObject());
    setupScriptEngineForFile(m_engine, m_scanner->scanScript->fileContext, m_global);
}

QStringList UserDependencyScanner::collectSearchPaths(Artifact *artifact)
{
    return evaluate(artifact, m_scanner->searchPathsScript);
}

QStringList UserDependencyScanner::collectDependencies(FileResourceBase *file, const char *fileTags)
{
    Q_UNUSED(fileTags);
    // ### support user dependency scanners for file deps
    Artifact *artifact = dynamic_cast<Artifact *>(file);
    if (!artifact)
        return QStringList();
    return evaluate(artifact, m_scanner->scanScript);
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
    return m_scanner->scanScript->sourceCode;
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

QStringList UserDependencyScanner::evaluate(Artifact *artifact, const ScriptFunctionPtr &script)
{
    ScriptEngineActiveFlagGuard guard(m_engine);

    if (artifact->product.get() != m_product) {
        m_product = artifact->product.get();
        setupScriptEngineForProduct(m_engine, artifact->product.lock(),
                                    m_scanner->module, m_global, &m_observer);
    }

    QScriptValueList args;
    args.reserve(3);
    args.append(m_global.property(QString::fromLatin1("project")));
    args.append(m_global.property(QString::fromLatin1("product")));
    args.append(Transformer::translateFileConfig(m_engine, artifact, m_scanner->module->name));

    m_engine->setGlobalObject(m_global);
    QScriptValue &function = script->scriptFunction;
    if (!function.isValid() || function.engine() != m_engine) {
        function = m_engine->evaluate(script->sourceCode);
        if (Q_UNLIKELY(!function.isFunction()))
            throw ErrorInfo(Tr::tr("Invalid scan script."), script->location);
    }
    QScriptValue result = function.call(QScriptValue(), args);
    m_engine->setGlobalObject(m_global.prototype());
    m_engine->clearRequestedProperties();
    if (Q_UNLIKELY(m_engine->hasErrorOrException(result))) {
        QString msg = Tr::tr("evaluating scan script: ") + m_engine->lastErrorString(result);
        const CodeLocation loc = m_engine->lastErrorLocation(result, script->location);
        m_engine->clearExceptions();
        throw ErrorInfo(msg, loc);
    }
    QStringList list;
    if (result.isArray()) {
        const int count = result.property(QLatin1String("length")).toInt32();
        list.reserve(count);
        for (qint32 i = 0; i < count; ++i) {
            QScriptValue item = result.property(i);
            if (item.isValid() && !item.isUndefined())
                list.append(item.toString());
        }
    }
    return list;
}

} // namespace Internal
} // namespace qbs
