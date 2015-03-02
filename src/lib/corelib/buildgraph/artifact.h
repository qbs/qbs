/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
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

#ifndef QBS_ARTIFACT_H
#define QBS_ARTIFACT_H

#include "artifactset.h"
#include "filedependency.h"
#include "buildgraphnode.h"
#include "forward_decls.h"
#include <language/filetags.h>
#include <tools/filetime.h>

#include <QSet>
#include <QString>

namespace qbs {
namespace Internal {
class Logger;

/**
 * The Artifact class
 *
 * Let artifact P be the parent of artifact C. Thus C is child of P.
 * C produces P using the transformer P.transformer.
 *
 *
 */
class Artifact : public FileResourceBase, public BuildGraphNode
{
public:
    Artifact();
    ~Artifact();

    Type type() const { return ArtifactNodeType; }
    void accept(BuildGraphVisitor *visitor);
    QString toString() const;

    void addFileTag(const FileTag &t);
    void removeFileTag(const FileTag &t);
    void setFileTags(const FileTags &newFileTags);
    const FileTags &fileTags() const { return m_fileTags; }

    ArtifactSet childrenAddedByScanner;
    QSet<FileDependency *> fileDependencies;
    TransformerPtr transformer;
    PropertyMapPtr properties;

    enum ArtifactType
    {
        Unknown = 1,
        SourceFile = 2,
        Generated = 4
    };

    ArtifactType artifactType;
    bool inputsScanned : 1;                 // Do not serialize. Will be refreshed for every build.
    bool timestampRetrieved : 1;            // Do not serialize. Will be refreshed for every build.
    bool alwaysUpdated : 1;
    bool oldDataPossiblyPresent : 1;

    void initialize();
    ArtifactSet parentArtifacts() const;
    ArtifactSet childArtifacts() const;
    void onChildDisconnected(BuildGraphNode *child);

private:
    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

    FileTags m_fileTags;
};

// debugging helper
inline QString toString(Artifact::ArtifactType t)
{
    switch (t) {
    case Artifact::SourceFile:
        return QLatin1String("SourceFile");
    case Artifact::Generated:
        return QLatin1String("Generated");
    case Artifact::Unknown:
    default:
        return QLatin1String("Unknown");
    }
}

// debugging helper
inline QString toString(BuildGraphNode::BuildState s)
{
    switch (s) {
    case BuildGraphNode::Untouched:
        return QLatin1String("Untouched");
    case BuildGraphNode::Buildable:
        return QLatin1String("Buildable");
    case BuildGraphNode::Building:
        return QLatin1String("Building");
    case BuildGraphNode::Built:
        return QLatin1String("Built");
    default:
        return QLatin1String("Unknown");
    }
}

} // namespace Internal
} // namespace qbs

#endif // QBS_ARTIFACT_H
