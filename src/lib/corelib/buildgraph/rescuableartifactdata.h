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

#ifndef QBS_RESCUABLEARTIFACTDATA_H
#define QBS_RESCUABLEARTIFACTDATA_H

#include "forward_decls.h"

#include <language/filetags.h>
#include <language/forward_decls.h>
#include <language/property.h>
#include <tools/filetime.h>

#include <QtCore/qhash.h>
#include <QtCore/qlist.h>

namespace qbs {
namespace Internal {
class PersistentPool;

class QBS_AUTOTEST_EXPORT RescuableArtifactData
{
public:
    ~RescuableArtifactData();

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

    struct ChildData
    {
        ChildData(const QString &n = QString(), const QString &m = QString(),
                  const QString &c = QString(), bool byScanner = false)
            : productName(n), productMultiplexId(m), childFilePath(c), addedByScanner(byScanner)
        {}

        void load(PersistentPool &pool)
        {
            pool.load(productName);
            pool.load(productMultiplexId);
            pool.load(childFilePath);
            pool.load(addedByScanner);
        }

        void store(PersistentPool &pool) const
        {
            pool.store(productName);
            pool.store(productMultiplexId);
            pool.store(childFilePath);
            pool.store(addedByScanner);
        }

        QString productName;
        QString productMultiplexId;
        QString childFilePath;
        bool addedByScanner;
    };

    FileTime timeStamp;
    QList<ChildData> children;

    // Per-Transformer data
    QList<AbstractCommandPtr> commands;
    PropertySet propertiesRequestedInPrepareScript;
    PropertySet propertiesRequestedInCommands;
    PropertyHash propertiesRequestedFromArtifactInPrepareScript;
    PropertyHash propertiesRequestedFromArtifactInCommands;

    // Only needed for API purposes
    FileTags fileTags;
    PropertyMapPtr properties;

};
typedef QHash<QString, RescuableArtifactData> AllRescuableArtifactData;

} // namespace Internal
} // namespace qbs

#endif // Include guard.
