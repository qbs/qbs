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

#include "artifactset.h"
#include "artifact.h"

namespace qbs {
namespace Internal {

ArtifactSet::ArtifactSet()
{
}

ArtifactSet::ArtifactSet(const ArtifactSet &other)
    : QSet<Artifact *>(other)
{
}

ArtifactSet::ArtifactSet(const QSet<Artifact *> &other)
    : QSet<Artifact *>(other)
{
}

ArtifactSet &ArtifactSet::unite(const ArtifactSet &other)
{
    QSet<Artifact *>::unite(other);
    return *this;
}

QStringList ArtifactSet::toStringList() const
{
    QStringList sl;
    foreach (Artifact *a, *this)
        sl += a->filePath();
    return sl;
}

QString ArtifactSet::toString() const
{
    return QLatin1Char('[') + toStringList().join(QLatin1String(", ")) + QLatin1Char(']');
}

ArtifactSet ArtifactSet::fromNodeSet(const NodeSet &nodes)
{
    ArtifactSet result;
    result.reserve(nodes.count());
    foreach (BuildGraphNode *node, nodes) {
        Artifact *artifact = dynamic_cast<Artifact *>(node);
        if (artifact)
            result += artifact;
    }
    return result;
}

ArtifactSet ArtifactSet::fromNodeList(const QList<Artifact *> &lst)
{
    ArtifactSet result;
    result.reserve(lst.count());
    for (QList<Artifact *>::const_iterator it = lst.constBegin(); it != lst.end(); ++it)
        result.insert(*it);
    return result;
}

} // namespace Internal
} // namespace qbs
