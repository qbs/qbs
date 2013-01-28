/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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
#ifndef QBS_LANG_FORWARD_DECLS_H
#define QBS_LANG_FORWARD_DECLS_H

#include <QSharedPointer>

namespace qbs {
namespace Internal {

class PropertyMap;
typedef QSharedPointer<PropertyMap> PropertyMapPtr;
typedef QSharedPointer<const PropertyMap> PropertyMapConstPtr;

class FileTagger;
typedef QSharedPointer<FileTagger> FileTaggerPtr;
typedef QSharedPointer<const FileTagger> FileTaggerConstPtr;

class ResolvedProduct;
typedef QSharedPointer<ResolvedProduct> ResolvedProductPtr;
typedef QSharedPointer<const ResolvedProduct> ResolvedProductConstPtr;

class ResolvedProject;
typedef QSharedPointer<ResolvedProject> ResolvedProjectPtr;
typedef QSharedPointer<const ResolvedProject> ResolvedProjectConstPtr;

class Rule;
typedef QSharedPointer<Rule> RulePtr;
typedef QSharedPointer<const Rule> RuleConstPtr;

class SourceArtifact;
typedef QSharedPointer<SourceArtifact> SourceArtifactPtr;
typedef QSharedPointer<const SourceArtifact> SourceArtifactConstPtr;

class PrepareScript;
typedef QSharedPointer<PrepareScript> PrepareScriptPtr;
typedef QSharedPointer<const PrepareScript> PrepareScriptConstPtr;

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
