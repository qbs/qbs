/*************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

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
    , outOfDateCheckPerformed(false)
    , isOutOfDate(false)
    , isExistingFile(false)
{
}

Artifact::~Artifact()
{
}

void Artifact::load(PersistentPool &pool, PersistentObjectData &data)
{
    QDataStream s(data);
    fileName = pool.idLoadString(s);
    fileTags = pool.idLoadStringSet(s);
    configuration = pool.idLoadS<Configuration>(s);
    transformer = pool.idLoadS<Transformer>(s);
    s >> artifactType;
    product = pool.idLoadS<BuildProduct>(s).data();
}

void Artifact::store(PersistentPool &pool, PersistentObjectData &data) const
{
    QDataStream s(&data, QIODevice::WriteOnly);
    s << pool.storeString(fileName);
    s << pool.storeStringSet(fileTags);
    s << pool.store(configuration);
    s << pool.store(transformer);
    s << artifactType;
    s << pool.store(product);
}

} // namespace qbs
