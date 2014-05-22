/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "depscanner.h"
#include "artifact.h"
#include "projectbuilddata.h"
#include "buildgraph.h"

#include <tools/error.h>
#include <logging/translator.h>
#include <language/language.h>
#include <language/scriptengine.h>
#include <jsextensions/moduleproperties.h>
#include <plugins/scanner/scanner.h>

#include <QVariantMap>
#include <QSet>
#include <QScriptContext>

namespace qbs {
namespace Internal {

static void collectCppIncludePaths(const QVariantMap &modules, QSet<QString> *collectedPaths)
{
    QMapIterator<QString, QVariant> iterator(modules);
    while (iterator.hasNext()) {
        iterator.next();
        if (iterator.key() == QLatin1String("cpp")) {
            QVariant includePathsVariant =
                    iterator.value().toMap().value(QLatin1String("includePaths"));
            if (includePathsVariant.isValid())
                collectedPaths->unite(QSet<QString>::fromList(includePathsVariant.toStringList()));
        } else {
            collectCppIncludePaths(iterator.value().toMap().value(QLatin1String("modules")).toMap(),
                                collectedPaths);
        }
    }
}

PluginDependencyScanner::PluginDependencyScanner(ScannerPlugin *plugin)
    : m_plugin(plugin)
{
    m_fileTag += FileTag(plugin->fileTag);
}

QStringList PluginDependencyScanner::collectSearchPaths(Artifact *artifact)
{
    if (m_plugin->flags & ScannerUsesCppIncludePaths) {
        QVariantMap modules = artifact->properties->value().value(QLatin1String("modules")).toMap();
        QSet<QString> collectedPaths;
        collectCppIncludePaths(modules, &collectedPaths);
        return QStringList(collectedPaths.toList());
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

const FileTags &PluginDependencyScanner::fileTags()
{
    return m_fileTag;
}

bool PluginDependencyScanner::recursive()
{
    return m_plugin->flags & ScannerRecursiveDependencies;
}

void *PluginDependencyScanner::key()
{
    return m_plugin;
}

UserDependencyScanner::UserDependencyScanner(const ResolvedScannerConstPtr &scanner,
        const Logger &logger, ScriptEngine *engine)
    : m_scanner(scanner),
      m_logger(logger),
      m_engine(engine),
      m_observer(engine),
      m_project(0),
      m_product(0)
{
    m_global = engine->newObject();
    setupScriptEngineForFile(m_engine, m_scanner->scanScript->fileContext, m_global);
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

const FileTags &UserDependencyScanner::fileTags()
{
    return m_scanner->inputs;
}

bool UserDependencyScanner::recursive()
{
    return m_scanner->recursive;
}

void *UserDependencyScanner::key()
{
    return const_cast<ResolvedScanner*>(m_scanner.data());
}

QStringList UserDependencyScanner::evaluate(Artifact *artifact, const ScriptFunctionPtr &script)
{
    if ((artifact->product.data() != m_product) || (artifact->product->project.data() != m_project)) {
        m_product = artifact->product.data();
        m_project = artifact->product->project.data();
        setupScriptEngineForProduct(m_engine, artifact->product,
                                    m_scanner->module, m_global, &m_observer);
    }

    QScriptValue artifactConfig = m_engine->newObject();
    ModuleProperties::init(artifactConfig, artifact);
    artifactConfig.setProperty(QLatin1String("fileName"), artifact->filePath(), 0);
    const QStringList fileTags = artifact->fileTags.toStringList();
    artifactConfig.setProperty(QLatin1String("fileTags"), m_engine->toScriptValue(fileTags));
    if (!m_scanner->module->name.isEmpty())
        artifactConfig.setProperty(QLatin1String("moduleName"), m_scanner->module->name);
    QScriptValueList args;
    args.reserve(3);
    args.append(m_global.property(QString::fromLatin1("project")));
    args.append(m_global.property(QString::fromLatin1("product")));
    args.append(artifactConfig);

    QScriptContext *ctx = m_engine->currentContext();
    ctx->pushScope(m_global);
    QScriptValue &function = script->scriptFunction;
    if (!function.isValid() || function.engine() != m_engine) {
        function = m_engine->evaluate(script->sourceCode);
        if (Q_UNLIKELY(!function.isFunction()))
            throw ErrorInfo(Tr::tr("Invalid scan script."), script->location);
    }
    QScriptValue result = function.call(QScriptValue(), args);
    ctx->popScope();
    m_engine->clearRequestedProperties();
    if (Q_UNLIKELY(m_engine->hasErrorOrException(result))) {
        QString msg = Tr::tr("evaluating scan script: ") + m_engine->uncaughtException().toString();
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
