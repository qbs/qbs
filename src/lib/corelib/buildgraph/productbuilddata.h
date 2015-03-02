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
#ifndef QBS_PRODUCTBUILDDATA_H
#define QBS_PRODUCTBUILDDATA_H

#include "artifactset.h"
#include "nodeset.h"
#include "rescuableartifactdata.h"
#include <language/filetags.h>
#include <language/forward_decls.h>
#include <tools/persistentobject.h>

#include <QList>
#include <QSet>

namespace qbs {
namespace Internal {

class Logger;

class ProductBuildData : public PersistentObject
{
public:
    ~ProductBuildData();

    ArtifactSet rootArtifacts() const;
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
