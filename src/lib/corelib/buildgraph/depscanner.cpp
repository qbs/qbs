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

#include <tools/error.h>
#include <logging/translator.h>
#include <language/language.h>
#include <language/propertymapinternal.h>
#include <language/scriptengine.h>
#include <jsextensions/moduleproperties.h>
#include <plugins/scanner/scanner.h>
#include <tools/fileinfo.h>

#include <QVariantMap>
#include <QSet>
#include <QScriptContext>

namespace qbs {
namespace Internal {

static QStringList collectCppIncludePaths(const QVariantMap &modules)
{
    QStringList result;
    const QVariantMap cpp = modules.value(QLatin1String("cpp")).toMap();
    if (cpp.isEmpty())
        return result;

    result
        << cpp.value(QLatin1String("includePaths")).toStringList()
        << cpp.value(QLatin1String("systemIncludePaths")).toStringList()
        << cpp.value(QLatin1String("compilerIncludePaths")).toStringList();
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

QStringList PluginDependencyScanner::collectDependencies(FileResourceBase *file)
{
    QSet<QString> result;
    QString baseDirOfInFilePath = file->dirPath();
    const QString &filepath = file->filePath();
    void *scannerHandle = m_plugin->open(filepath.utf16(), ScanForDependenciesFlag);
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

UserDependencyScanner::UserDependencyScanner(const ResolvedScannerConstPtr &scanner,
        const Logger &logger)
    : m_scanner(scanner),
      m_logger(logger),
      m_engine(new ScriptEngine(m_logger, EvalContext::RuleExecution)),
      m_observer(m_engine),
      m_product(0)
{
    m_engine->setProcessEventsInterval(-1); // QBS-782
    m_global = m_engine->newObject();
    m_global.setPrototype(m_engine->globalObject());
    setupScriptEngineForFile(m_engine, m_scanner->scanScript->fileContext, m_global);
}

UserDependencyScanner::~UserDependencyScanner()
{
    delete m_engine;
}

QStringList UserDependencyScanner::collectSearchPaths(Artifact *artifact)
{
    return evaluate(artifact, m_scanner->searchPathsScript);
}

QStringList UserDependencyScanner::collectDependencies(FileResourceBase *file)
{
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
    return m_scanner.data();
}

QStringList UserDependencyScanner::evaluate(Artifact *artifact, const ScriptFunctionPtr &script)
{
    if (artifact->product.data() != m_product) {
        m_product = artifact->product.data();
        setupScriptEngineForProduct(m_engine, artifact->product,
                                    m_scanner->module, m_global, &m_observer);
    }

    QScriptValue artifactConfig = m_engine->newObject();
    ModuleProperties::init(artifactConfig, artifact);
    artifactConfig.setProperty(QLatin1String("fileName"),
                               FileInfo::fileName(artifact->filePath()), 0);
    artifactConfig.setProperty(QLatin1String("filePath"), artifact->filePath(), 0);
    const QStringList fileTags = artifact->fileTags().toStringList();
    artifactConfig.setProperty(QLatin1String("fileTags"), m_engine->toScriptValue(fileTags));
    if (!m_scanner->module->name.isEmpty())
        artifactConfig.setProperty(QLatin1String("moduleName"), m_scanner->module->name);
    QScriptValueList args;
    args.reserve(3);
    args.append(m_global.property(QString::fromLatin1("project")));
    args.append(m_global.property(QString::fromLatin1("product")));
    args.append(artifactConfig);

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
        m_engine->clearExceptions();
        throw ErrorInfo(msg, script->location);
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
