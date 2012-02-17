/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
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

#ifndef ARTIFACTLIST_H
#define ARTIFACTLIST_H

#include <set>

namespace qbs {

class Artifact;

/**
  * List that holds a bunch of build graph artifacts.
  * This is faster than QSet when iterating over the container.
  */
class ArtifactList
{
public:
    ArtifactList();
    ArtifactList(const ArtifactList &other);

    typedef std::set<Artifact *>::const_iterator const_iterator;
    typedef std::set<Artifact *>::iterator iterator;
    typedef Artifact * value_type;

    iterator begin() { return m_data.begin(); }
    iterator end() { return m_data.end(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator end() const { return m_data.end(); }

    void insert(Artifact *artifact)
    {
        m_data.insert(artifact);
    }

    void operator +=(Artifact *artifact)
    {
        insert(artifact);
    }

    void remove(Artifact *artifact);

    bool contains(Artifact *artifact) const
    {
        return m_data.find(artifact) != m_data.end();
    }

    void clear()
    {
        m_data.clear();
    }

    bool isEmpty() const
    {
        return m_data.empty();
    }

    size_t count() const
    {
        return m_data.size();
    }

    void reserve(int)
    {
        // no-op
    }

private:
    mutable std::set<Artifact *> m_data;
};

} // namespace qbs

#endif // ARTIFACTLIST_H
