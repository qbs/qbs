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
#ifndef QBS_LANG_FORWARD_DECLS_H
#define QBS_LANG_FORWARD_DECLS_H

#include <memory>

namespace qbs {
namespace Internal {

enum class ObserveMode;

class Value;
using ValuePtr = std::shared_ptr<Value>;
using ValueConstPtr = std::shared_ptr<const Value>;

class ItemValue;
using ItemValuePtr = std::shared_ptr<ItemValue>;
using ItemValueConstPtr = std::shared_ptr<const ItemValue>;

class JSSourceValue;
using JSSourceValuePtr = std::shared_ptr<JSSourceValue>;
using JSSourceValueConstPtr = std::shared_ptr<const JSSourceValue>;

class VariantValue;
using VariantValuePtr = std::shared_ptr<VariantValue>;
using VariantValueConstPtr = std::shared_ptr<const VariantValue>;

class FileContext;
using FileContextPtr = std::shared_ptr<FileContext>;
using FileContextConstPtr = std::shared_ptr<const FileContext>;

class FileContextBase;
using FileContextBasePtr = std::shared_ptr<FileContextBase>;
using FileContextBaseConstPtr = std::shared_ptr<const FileContextBase>;

class Probe;
using ProbePtr = std::shared_ptr<Probe>;
using ProbeConstPtr = std::shared_ptr<const Probe>;

class PropertyMapInternal;
using PropertyMapPtr = std::shared_ptr<PropertyMapInternal>;
using PropertyMapConstPtr = std::shared_ptr<const PropertyMapInternal>;

class FileTagger;
using FileTaggerPtr = std::shared_ptr<FileTagger>;
using FileTaggerConstPtr = std::shared_ptr<const FileTagger>;

class ResolvedProduct;
using ResolvedProductPtr = std::shared_ptr<ResolvedProduct>;
using ResolvedProductConstPtr = std::shared_ptr<const ResolvedProduct>;

class ResolvedProject;
using ResolvedProjectPtr = std::shared_ptr<ResolvedProject>;
using ResolvedProjectConstPtr = std::shared_ptr<const ResolvedProject>;

class TopLevelProject;
using TopLevelProjectPtr = std::shared_ptr<TopLevelProject>;
using TopLevelProjectConstPtr = std::shared_ptr<const TopLevelProject>;

class ResolvedFileContext;
using ResolvedFileContextPtr = std::shared_ptr<ResolvedFileContext>;
using ResolvedFileContextConstPtr = std::shared_ptr<const ResolvedFileContext>;

class Rule;
using RulePtr = std::shared_ptr<Rule>;
using RuleConstPtr = std::shared_ptr<const Rule>;

class ResolvedScanner;
using ResolvedScannerPtr = std::shared_ptr<ResolvedScanner>;
using ResolvedScannerConstPtr = std::shared_ptr<const ResolvedScanner>;

class SourceArtifactInternal;
using SourceArtifactPtr = std::shared_ptr<SourceArtifactInternal>;
using SourceArtifactConstPtr = std::shared_ptr<const SourceArtifactInternal>;

class ScriptFunction;
using ScriptFunctionPtr = std::shared_ptr<ScriptFunction>;
using ScriptFunctionConstPtr = std::shared_ptr<const ScriptFunction>;
class PrivateScriptFunction;

class RuleArtifact;
using RuleArtifactPtr = std::shared_ptr<RuleArtifact>;
using RuleArtifactConstPtr = std::shared_ptr<const RuleArtifact>;

class ResolvedModule;
using ResolvedModulePtr = std::shared_ptr<ResolvedModule>;
using ResolvedModuleConstPtr = std::shared_ptr<const ResolvedModule>;

class ResolvedGroup;
using GroupPtr = std::shared_ptr<ResolvedGroup>;
using GroupConstPtr = std::shared_ptr<const ResolvedGroup>;

class ArtifactProperties;
using ArtifactPropertiesPtr = std::shared_ptr<ArtifactProperties>;
using ArtifactPropertiesConstPtr = std::shared_ptr<const ArtifactProperties>;

class ExportedItem;
using ExportedItemPtr = std::shared_ptr<ExportedItem>;
class ExportedModule;
class ExportedModuleDependency;
class ExportedProperty;
class PersistentPool;

} // namespace Internal
} // namespace qbs

#ifdef QT_CORE_LIB
#include <QtCore/qhash.h>

namespace qbs {
namespace Internal {

template <typename T> inline static uint qHash(const std::shared_ptr<T> &p, uint seed = 0)
{
    return ::qHash(p.get(), seed);
}

} // namespace Internal
} // namespace qbs
#endif

#endif // QBS_LANG_FORWARD_DECLS_H
