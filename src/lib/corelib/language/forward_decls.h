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

#include <QtCore/qhash.h>

namespace qbs {
namespace Internal {

class Value;
typedef std::shared_ptr<Value> ValuePtr;
typedef std::shared_ptr<const Value> ValueConstPtr;

class ItemValue;
typedef std::shared_ptr<ItemValue> ItemValuePtr;
typedef std::shared_ptr<const ItemValue> ItemValueConstPtr;

class JSSourceValue;
typedef std::shared_ptr<JSSourceValue> JSSourceValuePtr;
typedef std::shared_ptr<const JSSourceValue> JSSourceValueConstPtr;

class VariantValue;
typedef std::shared_ptr<VariantValue> VariantValuePtr;
typedef std::shared_ptr<const VariantValue> VariantValueConstPtr;

class FileContext;
typedef std::shared_ptr<FileContext> FileContextPtr;
typedef std::shared_ptr<const FileContext> FileContextConstPtr;

class FileContextBase;
typedef std::shared_ptr<FileContextBase> FileContextBasePtr;
typedef std::shared_ptr<const FileContextBase> FileContextBaseConstPtr;

class Probe;
typedef std::shared_ptr<Probe> ProbePtr;
typedef std::shared_ptr<const Probe> ProbeConstPtr;

class PropertyMapInternal;
typedef std::shared_ptr<PropertyMapInternal> PropertyMapPtr;
typedef std::shared_ptr<const PropertyMapInternal> PropertyMapConstPtr;

class FileTagger;
typedef std::shared_ptr<FileTagger> FileTaggerPtr;
typedef std::shared_ptr<const FileTagger> FileTaggerConstPtr;

class ResolvedProduct;
typedef std::shared_ptr<ResolvedProduct> ResolvedProductPtr;
typedef std::shared_ptr<const ResolvedProduct> ResolvedProductConstPtr;

class ResolvedProject;
typedef std::shared_ptr<ResolvedProject> ResolvedProjectPtr;
typedef std::shared_ptr<const ResolvedProject> ResolvedProjectConstPtr;

class TopLevelProject;
typedef std::shared_ptr<TopLevelProject> TopLevelProjectPtr;
typedef std::shared_ptr<const TopLevelProject> TopLevelProjectConstPtr;

class ResolvedFileContext;
typedef std::shared_ptr<ResolvedFileContext> ResolvedFileContextPtr;
typedef std::shared_ptr<const ResolvedFileContext> ResolvedFileContextConstPtr;

class Rule;
typedef std::shared_ptr<Rule> RulePtr;
typedef std::shared_ptr<const Rule> RuleConstPtr;

class ResolvedScanner;
typedef std::shared_ptr<ResolvedScanner> ResolvedScannerPtr;
typedef std::shared_ptr<const ResolvedScanner> ResolvedScannerConstPtr;

class SourceArtifactInternal;
typedef std::shared_ptr<SourceArtifactInternal> SourceArtifactPtr;
typedef std::shared_ptr<const SourceArtifactInternal> SourceArtifactConstPtr;

class ScriptFunction;
typedef std::shared_ptr<ScriptFunction> ScriptFunctionPtr;
typedef std::shared_ptr<const ScriptFunction> ScriptFunctionConstPtr;

class RuleArtifact;
typedef std::shared_ptr<RuleArtifact> RuleArtifactPtr;
typedef std::shared_ptr<const RuleArtifact> RuleArtifactConstPtr;

class ResolvedModule;
typedef std::shared_ptr<ResolvedModule> ResolvedModulePtr;
typedef std::shared_ptr<const ResolvedModule> ResolvedModuleConstPtr;

class ResolvedGroup;
typedef std::shared_ptr<ResolvedGroup> GroupPtr;
typedef std::shared_ptr<const ResolvedGroup> GroupConstPtr;

class ArtifactProperties;
typedef std::shared_ptr<ArtifactProperties> ArtifactPropertiesPtr;
typedef std::shared_ptr<const ArtifactProperties> ArtifactPropertiesConstPtr;

template <typename T> inline static uint qHash(const std::shared_ptr<T> &p, uint seed = 0)
{
    return ::qHash(p.get(), seed);
}

} // namespace Internal
} // namespace qbs

#endif // QBS_LANG_FORWARD_DECLS_H
