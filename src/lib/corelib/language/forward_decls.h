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

#include <QSharedPointer>

namespace qbs {
namespace Internal {

class Value;
typedef QSharedPointer<Value> ValuePtr;
typedef QSharedPointer<const Value> ValueConstPtr;

class ItemValue;
typedef QSharedPointer<ItemValue> ItemValuePtr;
typedef QSharedPointer<const ItemValue> ItemValueConstPtr;

class JSSourceValue;
typedef QSharedPointer<JSSourceValue> JSSourceValuePtr;
typedef QSharedPointer<const JSSourceValue> JSSourceValueConstPtr;

class VariantValue;
typedef QSharedPointer<VariantValue> VariantValuePtr;
typedef QSharedPointer<const VariantValue> VariantValueConstPtr;

class FileContext;
typedef QSharedPointer<FileContext> FileContextPtr;
typedef QSharedPointer<const FileContext> FileContextConstPtr;

class FileContextBase;
typedef QSharedPointer<FileContextBase> FileContextBasePtr;
typedef QSharedPointer<const FileContextBase> FileContextBaseConstPtr;

class Probe;
typedef QSharedPointer<Probe> ProbePtr;
typedef QSharedPointer<const Probe> ProbeConstPtr;

class PropertyMapInternal;
typedef QSharedPointer<PropertyMapInternal> PropertyMapPtr;
typedef QSharedPointer<const PropertyMapInternal> PropertyMapConstPtr;

class FileTagger;
typedef QSharedPointer<FileTagger> FileTaggerPtr;
typedef QSharedPointer<const FileTagger> FileTaggerConstPtr;

class ResolvedProduct;
typedef QSharedPointer<ResolvedProduct> ResolvedProductPtr;
typedef QSharedPointer<const ResolvedProduct> ResolvedProductConstPtr;

class ResolvedProject;
typedef QSharedPointer<ResolvedProject> ResolvedProjectPtr;
typedef QSharedPointer<const ResolvedProject> ResolvedProjectConstPtr;

class TopLevelProject;
typedef QSharedPointer<TopLevelProject> TopLevelProjectPtr;
typedef QSharedPointer<const TopLevelProject> TopLevelProjectConstPtr;

class ResolvedFileContext;
typedef QSharedPointer<ResolvedFileContext> ResolvedFileContextPtr;
typedef QSharedPointer<const ResolvedFileContext> ResolvedFileContextConstPtr;

class Rule;
typedef QSharedPointer<Rule> RulePtr;
typedef QSharedPointer<const Rule> RuleConstPtr;

class ResolvedScanner;
typedef QSharedPointer<ResolvedScanner> ResolvedScannerPtr;
typedef QSharedPointer<const ResolvedScanner> ResolvedScannerConstPtr;

class SourceArtifactInternal;
typedef QSharedPointer<SourceArtifactInternal> SourceArtifactPtr;
typedef QSharedPointer<const SourceArtifactInternal> SourceArtifactConstPtr;

class ScriptFunction;
typedef QSharedPointer<ScriptFunction> ScriptFunctionPtr;
typedef QSharedPointer<const ScriptFunction> ScriptFunctionConstPtr;

class RuleArtifact;
typedef QSharedPointer<RuleArtifact> RuleArtifactPtr;
typedef QSharedPointer<const RuleArtifact> RuleArtifactConstPtr;

class ResolvedModule;
typedef QSharedPointer<ResolvedModule> ResolvedModulePtr;
typedef QSharedPointer<const ResolvedModule> ResolvedModuleConstPtr;

class ResolvedGroup;
typedef QSharedPointer<ResolvedGroup> GroupPtr;
typedef QSharedPointer<const ResolvedGroup> GroupConstPtr;

class ArtifactProperties;
typedef QSharedPointer<ArtifactProperties> ArtifactPropertiesPtr;
typedef QSharedPointer<const ArtifactProperties> ArtifactPropertiesConstPtr;

} // namespace Internal
} // namespace qbs

#endif // QBS_LANG_FORWARD_DECLS_H
