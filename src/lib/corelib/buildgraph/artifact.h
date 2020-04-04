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

#ifndef QBS_ARTIFACT_H
#define QBS_ARTIFACT_H

#include "filedependency.h"
#include "buildgraphnode.h"
#include "forward_decls.h"
#include <language/filetags.h>
#include <tools/dynamictypecheck.h>
#include <tools/filetime.h>
#include <tools/set.h>

#include <QtCore/qstring.h>

#include <utility>
#include <vector>

namespace qbs {
namespace Internal {
class Logger;

class Artifact;
using ArtifactSet = Set<Artifact *>;

/**
 * The Artifact class
 *
 * Let artifact P be the parent of artifact C. Thus C is child of P.
 * C produces P using the transformer P.transformer.
 *
 *
 */
class QBS_AUTOTEST_EXPORT Artifact : public FileResourceBase, public BuildGraphNode
{
public:
    Artifact();
    ~Artifact() override;

    Type type() const override { return ArtifactNodeType; }
    FileType fileType() const override { return FileTypeArtifact; }
    void accept(BuildGraphVisitor *visitor) override;
    QString toString() const override;

    void addFileTag(const FileTag &t);
    void removeFileTag(const FileTag &t);
    void setFileTags(const FileTags &newFileTags);
    const FileTags &fileTags() const { return m_fileTags; }

    RuleNode *producer() const;

    ArtifactSet childrenAddedByScanner;
    Set<FileDependency *> fileDependencies;
    TransformerPtr transformer;
    PropertyMapPtr properties;
    QString targetOfModule;

    // The tags set directly via an Artifact item or an outputArtifacts script,
    // not the result of file taggers or fileTagsFilter groups, nor the ones inherited from
    // the product.
    FileTags pureFileTags;

    // The properties attached directly to an artifact in an Artifact item or outputArtifacts
    // script.
    std::vector<std::pair<QStringList, QVariant>> pureProperties;

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

    const TypeFilter<Artifact> parentArtifacts() const;
    const TypeFilter<Artifact> childArtifacts() const;
    void onChildDisconnected(BuildGraphNode *child) override;

    bool isTargetOfModule() const { return !targetOfModule.isEmpty(); }

    void load(PersistentPool &pool) override;
    void store(PersistentPool &pool) override;

private:
    FileTags m_fileTags;
};

template<> inline QString Set<Artifact *>::toString(Artifact * const &artifact) const
{
    return artifact ? artifact->filePath() : QStringLiteral("<null>");
}
template<> inline const void *uniqueAddress(const Artifact *a)
{
    return static_cast<const BuildGraphNode *>(a);
}

template<> inline bool hasDynamicType<Artifact>(const BuildGraphNode *n)
{
    return n->type() == BuildGraphNode::ArtifactNodeType;
}

// debugging helper
inline QString toString(Artifact::ArtifactType t)
{
    switch (t) {
    case Artifact::SourceFile:
        return QStringLiteral("SourceFile");
    case Artifact::Generated:
        return QStringLiteral("Generated");
    case Artifact::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

// debugging helper
inline QString toString(BuildGraphNode::BuildState s)
{
    switch (s) {
    case BuildGraphNode::Untouched:
        return QStringLiteral("Untouched");
    case BuildGraphNode::Buildable:
        return QStringLiteral("Buildable");
    case BuildGraphNode::Building:
        return QStringLiteral("Building");
    case BuildGraphNode::Built:
        return QStringLiteral("Built");
    default:
        return QStringLiteral("Unknown");
    }
}

} // namespace Internal
} // namespace qbs

#endif // QBS_ARTIFACT_H
