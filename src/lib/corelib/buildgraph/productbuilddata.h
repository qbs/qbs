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
#ifndef QBS_PRODUCTBUILDDATA_H
#define QBS_PRODUCTBUILDDATA_H

#include "artifact.h"
#include "nodeset.h"
#include "rescuableartifactdata.h"
#include <language/filetags.h>
#include <language/forward_decls.h>
#include <tools/persistentobject.h>

#include <QtCore/qlist.h>

namespace qbs {
namespace Internal {

class Logger;

class QBS_AUTOTEST_EXPORT ProductBuildData : public PersistentObject
{
public:
    ~ProductBuildData();

    const TypeFilter<Artifact> rootArtifacts() const;
    NodeSet nodes;
    NodeSet roots;

    // After change tracking, this is the relevant data of artifacts that were in the build data
    // of the restored product, and will potentially be re-created by our rules.
    // If and when that happens, the relevant data will be copied over to the newly created
    // artifact.
    AllRescuableArtifactData rescuableArtifactData;

    // Do not store, initialized in executor. Higher prioritized artifacts are built first.
    unsigned int buildPriority;

    typedef QHash<FileTag, ArtifactSet> ArtifactSetByFileTag;
    ArtifactSetByFileTag artifactsByFileTag;

    typedef QHash<RuleConstPtr, ArtifactSet> ArtifactSetByRule;
    ArtifactSetByRule artifactsWithChangedInputsPerRule;

private:
    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
};

void addArtifactToSet(Artifact *artifact, ProductBuildData::ArtifactSetByFileTag &container);
void removeArtifactFromSetByFileTag(Artifact *artifact, const FileTag &fileTag,
        ProductBuildData::ArtifactSetByFileTag &container);
void removeArtifactFromSet(Artifact *artifact, ProductBuildData::ArtifactSetByFileTag &container);

} // namespace Internal
} // namespace qbs

#endif // QBS_PRODUCTBUILDDATA_H
