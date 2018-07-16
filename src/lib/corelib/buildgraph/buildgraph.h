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
#ifndef QBS_BUILDGRAPH_H
#define QBS_BUILDGRAPH_H

#include "forward_decls.h"
#include <language/forward_decls.h>
#include <tools/qbs_export.h>

#include <QtCore/qstringlist.h>

#include <QtScript/qscriptvalue.h>

namespace qbs {
namespace Internal {
class BuildGraphNode;
class Logger;
class ScriptEngine;
class PrepareScriptObserver;

Artifact *lookupArtifact(const ResolvedProductConstPtr &product,
                         const ProjectBuildData *projectBuildData,
                         const QString &dirPath, const QString &fileName,
                         bool compareByName = false);
Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const QString &dirPath,
                         const QString &fileName, bool compareByName = false);
Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const ProjectBuildData *buildData,
                         const QString &filePath, bool compareByName = false);
Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const QString &filePath,
                         bool compareByName = false);
Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const Artifact *artifact,
                         bool compareByName);

Artifact *createArtifact(const ResolvedProductPtr &product,
                         const SourceArtifactConstPtr &sourceArtifact);
void setArtifactData(Artifact *artifact, const SourceArtifactConstPtr &sourceArtifact);
void updateArtifactFromSourceArtifact(const ResolvedProductPtr &product,
                    const SourceArtifactConstPtr &sourceArtifact);
void insertArtifact(const ResolvedProductPtr &product, Artifact *artifact);
void dumpProductBuildData(const ResolvedProductConstPtr &product);
void provideFullFileTagsAndProperties(Artifact *artifact);
void applyPerArtifactProperties(Artifact *artifact);
void updateGeneratedArtifacts(ResolvedProduct *product);
void invalidateArtifactAsRuleInputIfNecessary(Artifact *artifact);

bool findPath(BuildGraphNode *u, BuildGraphNode *v, QList<BuildGraphNode*> &path);
void QBS_AUTOTEST_EXPORT connect(BuildGraphNode *p, BuildGraphNode *c);
bool safeConnect(Artifact *u, Artifact *v);
void removeGeneratedArtifactFromDisk(Artifact *artifact, const Logger &logger);
void removeGeneratedArtifactFromDisk(const QString &filePath, const Logger &logger);

void disconnect(BuildGraphNode *u, BuildGraphNode *v);

void setupScriptEngineForFile(ScriptEngine *engine, const FileContextBaseConstPtr &fileContext,
        QScriptValue targetObject, const ObserveMode &observeMode);
void setupScriptEngineForProduct(ScriptEngine *engine, ResolvedProduct *product,
                                 const ResolvedModule *module, QScriptValue targetObject,
                                 bool setBuildEnvironment);
QString relativeArtifactFileName(const Artifact *artifact); // Debugging helpers

void doSanityChecks(const ResolvedProjectPtr &project, const Logger &logger);

} // namespace Internal
} // namespace qbs

#endif // QBS_BUILDGRAPH_H
