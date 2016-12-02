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

#include "rescuableartifactdata.h"

#include "command.h"

#include <tools/persistence.h>

namespace qbs {
namespace Internal {

RescuableArtifactData::~RescuableArtifactData()
{
}

void RescuableArtifactData::load(PersistentPool &pool)
{
    pool.stream() >> timeStamp;

    int c;
    pool.stream() >> c;
    for (int i = 0; i < c; ++i) {
        ChildData cd;
        cd.productName = pool.idLoadString();
        cd.productProfile = pool.idLoadString();
        cd.childFilePath = pool.idLoadString();
        pool.stream() >> cd.addedByScanner;
        children << cd;
    }

    propertiesRequestedInPrepareScript = restorePropertySet(pool);
    propertiesRequestedInCommands = restorePropertySet(pool);
    propertiesRequestedFromArtifactInPrepareScript = restorePropertyHash(pool);
    propertiesRequestedFromArtifactInCommands = restorePropertyHash(pool);
    commands = loadCommandList(pool);
    fileTags.load(pool);
    properties = pool.loadVariantMap();
}

void RescuableArtifactData::store(PersistentPool &pool) const
{
    pool.stream() << timeStamp;

    pool.stream() << children.count();
    foreach (const ChildData &cd, children) {
        pool.storeString(cd.productName);
        pool.storeString(cd.productProfile);
        pool.storeString(cd.childFilePath);
        pool.stream() << cd.addedByScanner;
    }

    storePropertySet(pool, propertiesRequestedInPrepareScript);
    storePropertySet(pool, propertiesRequestedInCommands);
    storePropertyHash(pool, propertiesRequestedFromArtifactInPrepareScript);
    storePropertyHash(pool, propertiesRequestedFromArtifactInCommands);
    storeCommandList(commands, pool);
    fileTags.store(pool);
    pool.store(properties);
}

} // namespace Internal
} // namespace qbs
