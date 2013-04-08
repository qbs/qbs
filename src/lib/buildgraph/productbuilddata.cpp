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
#include "productbuilddata.h"

#include "artifact.h"
#include "projectbuilddata.h"
#include <language/language.h>
#include <logging/logger.h>
#include <tools/error.h>
#include <tools/persistence.h>
#include <tools/qbsassert.h>

namespace qbs {
namespace Internal {

ProductBuildData::~ProductBuildData()
{
    qDeleteAll(artifacts);
}

void ProductBuildData::load(PersistentPool &pool)
{
    // artifacts
    int i;
    pool.stream() >> i;
    artifacts.clear();
    for (; --i >= 0;) {
        Artifact *artifact = pool.idLoad<Artifact>();
        artifacts.insert(artifact);
    }

    // edges
    for (i = artifacts.count(); --i >= 0;) {
        Artifact *artifact = pool.idLoad<Artifact>();
        int k;
        pool.stream() >> k;
        artifact->parents.clear();
        artifact->parents.reserve(k);
        for (; --k >= 0;)
            artifact->parents.insert(pool.idLoad<Artifact>());

        pool.stream() >> k;
        artifact->children.clear();
        artifact->children.reserve(k);
        for (; --k >= 0;)
            artifact->children.insert(pool.idLoad<Artifact>());

        pool.stream() >> k;
        artifact->fileDependencies.clear();
        artifact->fileDependencies.reserve(k);
        for (; --k >= 0;)
            artifact->fileDependencies.insert(pool.idLoad<Artifact>());
    }

    // other data
    pool.loadContainer(targetArtifacts);
}

void ProductBuildData::store(PersistentPool &pool) const
{
    pool.stream() << artifacts.count();

    //artifacts
    for (ArtifactList::const_iterator i = artifacts.constBegin(); i != artifacts.constEnd(); ++i)
        pool.store(*i);

    // edges
    for (ArtifactList::const_iterator i = artifacts.constBegin(); i != artifacts.constEnd(); ++i) {
        Artifact * artifact = *i;
        pool.store(artifact);

        pool.stream() << artifact->parents.count();
        foreach (Artifact * n, artifact->parents)
            pool.store(n);
        pool.stream() << artifact->children.count();
        foreach (Artifact * n, artifact->children)
            pool.store(n);
        pool.stream() << artifact->fileDependencies.count();
        foreach (Artifact * n, artifact->fileDependencies)
            pool.store(n);
    }

    // other data
    pool.storeContainer(targetArtifacts);
}

} // namespace Internal
} // namespace qbs
