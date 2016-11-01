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

#include "filetags.h"
#include <QStringList>

#include <tools/persistence.h>

namespace qbs {
namespace Internal {

void FileTag::clear()
{
    Id::operator=(Id());
}

QStringList FileTags::toStringList() const
{
    QStringList strlst;
    foreach (const FileTag &tag, *this)
        strlst += tag.toString();
    return strlst;
}

FileTags FileTags::fromStringList(const QStringList &strings)
{
    FileTags result;
    foreach (const QString &str, strings)
        result += FileTag(str.toLocal8Bit());
    return result;
}

/*!
 * \return \c{true} if this file tags set has file tags in common with \c{other}.
 */
bool FileTags::matches(const FileTags &other) const
{
    for (FileTags::const_iterator it = other.begin(); it != other.end(); ++it)
        if (contains(*it))
            return true;
    return false;
}

void FileTags::store(PersistentPool &pool) const
{
    pool.storeStringList(toStringList());
}

void FileTags::load(PersistentPool &pool)
{
    *this = fromStringList(pool.idLoadStringList());
}

LogWriter operator <<(LogWriter w, const FileTags &tags)
{
    bool firstLoop = true;
    w.write('(');
    foreach (const FileTag &tag, tags) {
        if (firstLoop)
            firstLoop = false;
        else
            w.write(QLatin1String(", "));
        w.write(tag.toString());
    }
    w.write(')');
    return w;
}

} // namespace Internal
} // namespace qbs
