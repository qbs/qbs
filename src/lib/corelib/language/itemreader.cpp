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

#include "itemreader.h"

#include "itemreadervisitorstate.h"

#include <tools/profiling.h>

namespace qbs {
namespace Internal {

ItemReader::ItemReader(Logger &logger) : m_visitorState(new ItemReaderVisitorState(logger))
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
    m_extraSearchPaths.push_back(extraSearchPaths);
}

void ItemReader::popExtraSearchPaths()
{
    m_extraSearchPaths.pop_back();
}

std::vector<QStringList> ItemReader::extraSearchPathsStack() const
{
    return m_extraSearchPaths;
}

QStringList ItemReader::searchPaths() const
{
    QStringList paths;
    for (auto it = m_extraSearchPaths.crbegin(), end = m_extraSearchPaths.crend(); it != end; ++it)
        paths += *it;
    paths += m_searchPaths;
    return paths;
}

Item *ItemReader::readFile(const QString &filePath)
{
    AccumulatingTimer readFileTimer(m_elapsedTime != -1 ? &m_elapsedTime : nullptr);
    return m_visitorState->readFile(filePath, searchPaths(), m_pool);
}

Set<QString> ItemReader::filesRead() const
{
    return m_visitorState->filesRead();
}

void ItemReader::setEnableTiming(bool on)
{
    m_elapsedTime = on ? 0 : -1;
}

} // namespace Internal
} // namespace qbs
