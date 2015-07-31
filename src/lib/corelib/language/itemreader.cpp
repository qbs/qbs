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

#include "itemreader.h"

#include "itemreadervisitorstate.h"

namespace qbs {
namespace Internal {

ItemReader::ItemReader(const Logger &logger) : m_visitorState(new ItemReaderVisitorState(logger))
{
}

ItemReader::~ItemReader()
{
    delete m_visitorState;
}

void ItemReader::setSearchPaths(const QStringList &searchPaths)
{
    m_searchPaths = searchPaths;
}

void ItemReader::pushExtraSearchPaths(const QStringList &extraSearchPaths)
{
    m_extraSearchPaths.push(extraSearchPaths);
}

void ItemReader::popExtraSearchPaths()
{
    m_extraSearchPaths.pop();
}

QStack<QStringList> ItemReader::extraSearchPathsStack() const
{
    return m_extraSearchPaths;
}

QStringList ItemReader::searchPaths() const
{
    QStringList paths;
    for (int i = m_extraSearchPaths.count(); --i >= 0;)
        paths += m_extraSearchPaths.at(i);
    paths += m_searchPaths;
    return paths;
}

Item *ItemReader::readFile(const QString &filePath)
{
    return m_visitorState->readFile(filePath, searchPaths(), m_pool);
}

QSet<QString> ItemReader::filesRead() const
{
    return m_visitorState->filesRead();
}

} // namespace Internal
} // namespace qbs
