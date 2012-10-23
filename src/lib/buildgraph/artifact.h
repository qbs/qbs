/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#ifndef ARTIFACT_H
#define ARTIFACT_H

#include "artifactlist.h"
#include <language/language.h>

#include <QSet>
#include <QString>

namespace qbs {

class BuildProduct;
class BuildProject;
class Transformer;

/**
 * The Artifact class
 *
 * Let artifact P be the parent of artifact C. Thus C is child of P.
 * C produces P using the transformer P.transformer.
 *
 *
 */
class Artifact : public PersistentObject
{
public:
    Artifact(BuildProject *p = 0);
    ~Artifact();

    ArtifactList parents;
    ArtifactList children;
    ArtifactList fileDependencies;
    ArtifactList sideBySideArtifacts;     /// all artifacts that have been produced by the same rule
    QSet<QString> fileTags;
    BuildProject *project;
    BuildProduct *product;          // Note: file dependency artifacts don't belong to a product.
    QSharedPointer<Transformer> transformer;
    PropertyMap::Ptr properties;

    enum ArtifactType
    {
        Unknown,
        SourceFile,
        Generated,
        FileDependency
    };
    ArtifactType artifactType;

    enum BuildState
    {
        Untouched = 0,
        Buildable,
        Building,
        Built
    };
    BuildState buildState;

    bool inputsScanned : 1;

    // the following members are not serialized
    bool outOfDateCheckPerformed : 1;
    bool isOutOfDate : 1;
    bool isExistingFile : 1;

    void setFilePath(const QString &filePath);
    const QString &filePath() const { return m_filePath; }
    QString dirPath() const { return m_dirPath.toString(); }
    QString fileName() const { return m_fileName.toString(); }

private:
    void load(PersistentPool &pool, QDataStream &s);
    void store(PersistentPool &pool, QDataStream &s) const;

private:
    QString m_filePath;
    QStringRef m_dirPath;
    QStringRef m_fileName;
};

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

} // namespace qbs

#endif // ARTIFACT_H
