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
#include <QtCore/qstringlist.h>

#include <tools/persistence.h>

namespace qbs {
namespace Internal {

void FileTag::clear()
{
    Id::operator=(Id());
}

void FileTag::store(PersistentPool &pool) const
{
    pool.store(toString());
}

void FileTag::load(PersistentPool &pool)
{
    *this = FileTag(pool.load<QString>().toUtf8());
}

QDebug operator<<(QDebug debug, const FileTag &tag)
{
    QDebugStateSaver saver(debug);
    return debug.resetFormat().noquote() << tag.toString();
}

FileTags FileTags::fromStringList(const QStringList &strings)
{
    FileTags result;
    for (const QString &str : strings)
        result += FileTag(str.toUtf8());
    return result;
}

LogWriter operator <<(LogWriter w, const FileTags &tags)
{
    bool firstLoop = true;
    w.write('(');
    for (const FileTag &tag : tags) {
        if (firstLoop)
            firstLoop = false;
        else
            w.write(QStringLiteral(", "));
        w.write(tag.toString());
    }
    w.write(')');
    return w;
}

QDebug operator<<(QDebug debug, const FileTags &tags)
{
    QDebugStateSaver saver(debug);
    return debug.resetFormat().noquote() << tags.toStringList();
}

} // namespace Internal
} // namespace qbs
