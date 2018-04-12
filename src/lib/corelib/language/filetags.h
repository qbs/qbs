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

#ifndef QBS_FILETAGS_H
#define QBS_FILETAGS_H

#include <logging/logger.h>
#include <tools/id.h>
#include <tools/set.h>

#include <QtCore/qdatastream.h>

namespace qbs {
namespace Internal {
class PersistentPool;

class FileTag : public Id
{
public:
    FileTag()
        : Id()
    {}

    FileTag(const Id &other)
        : Id(other)
    {}

    FileTag(const char *str)
        : Id(str)
    {}

    explicit FileTag(const QByteArray &ba)
        : Id(ba)
    {}

    void clear();

    void store(PersistentPool &pool) const;
    void load(PersistentPool &pool);
};

template<> inline bool Set<FileTag>::sortAfterLoadRequired() const { return true; }
QDebug operator<<(QDebug debug, const FileTag &tag);

class FileTags : public Set<FileTag>
{
public:
    FileTags() : Set<FileTag>() {}
    FileTags(const std::initializer_list<FileTag> &list) : Set<FileTag>(list) {}
    static FileTags fromStringList(const QStringList &strings);
};

LogWriter operator <<(LogWriter w, const FileTags &tags);
QDebug operator<<(QDebug debug, const FileTags &tags);

} // namespace Internal
} // namespace qbs

#endif // QBS_FILETAGS_H

