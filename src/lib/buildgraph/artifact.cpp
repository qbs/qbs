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

#include "artifact.h"
#include "transformer.h"
#include "buildgraph.h"

QT_BEGIN_NAMESPACE

static QDataStream &operator >>(QDataStream &s, qbs::Artifact::ArtifactType &t)
{
    int i;
    s >> i;
    t = static_cast<qbs::Artifact::ArtifactType>(i);
    return s;
}

static QDataStream &operator <<(QDataStream &s, const qbs::Artifact::ArtifactType &t)
{
    return s << (int)t;
}

QT_END_NAMESPACE

namespace qbs {

Artifact::Artifact(BuildProject *p)
    : project(p)
    , product(0)
    , transformer(0)
    , artifactType(Unknown)
    , buildState(Untouched)
    , inputsScanned(false)
    , outOfDateCheckPerformed(false)
    , isOutOfDate(false)
    , isExistingFile(false)
{
}

Artifact::~Artifact()
{
}

void Artifact::setFilePath(const QString &filePath)
{
    m_filePath = filePath;
    FileInfo::splitIntoDirectoryAndFileName(m_filePath, &m_dirPath, &m_fileName);
}

void Artifact::load(PersistentPool &pool, QDataStream &s)
{
    setFilePath(pool.idLoadString());
    fileTags = pool.idLoadStringSet();
    configuration = pool.idLoadS<Configuration>(s);
    transformer = pool.idLoadS<Transformer>(s);
    quint8 n;
    s >> artifactType
      >> n;
    inputsScanned = n;
}

void Artifact::store(PersistentPool &pool, QDataStream &s) const
{
    pool.storeString(m_filePath);
    pool.storeStringSet(fileTags);
    pool.store(configuration);
    pool.store(transformer);
    s << artifactType
      << static_cast<quint8>(inputsScanned);
}

} // namespace qbs
