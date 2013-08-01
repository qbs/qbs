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

#ifndef QBS_ARTIFACT_H
#define QBS_ARTIFACT_H

#include "artifactlist.h"
#include "filedependency.h"
#include "forward_decls.h"
#include <language/filetags.h>
#include <language/forward_decls.h>
#include <tools/filetime.h>
#include <tools/persistentobject.h>
#include <tools/weakpointer.h>

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
class Artifact : public FileResourceBase
{
public:
    Artifact();
    ~Artifact();

    ArtifactList parents;
    ArtifactList children;
    QSet<FileDependency *> fileDependencies;
    FileTags fileTags;
    WeakPointer<ResolvedProduct> product;
    TransformerPtr transformer;
    PropertyMapPtr properties;

    enum ArtifactType
    {
        Unknown = 1,
        SourceFile = 2,
        Generated = 4
    };

    enum BuildState
    {
        Untouched = 0,
        Buildable,
        Building,
        Built
    };

    ArtifactType artifactType;
    FileTime autoMocTimestamp;
    BuildState buildState;                  // Do not serialize. Will be refreshed for every build.
    bool inputsScanned : 1;                 // Do not serialize. Will be refreshed for every build.
    bool timestampRetrieved : 1;            // Do not serialize. Will be refreshed for every build.
    bool alwaysUpdated : 1;

    void initialize();

private:
    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
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
inline QString toString(Artifact::BuildState s)
{
    switch (s) {
    case Artifact::Untouched:
        return QLatin1String("Untouched");
    case Artifact::Buildable:
        return QLatin1String("Buildable");
    case Artifact::Building:
        return QLatin1String("Building");
    case Artifact::Built:
        return QLatin1String("Built");
    default:
        return QLatin1String("Unknown");
    }
}

} // namespace Internal
} // namespace qbs

#endif // QBS_ARTIFACT_H
