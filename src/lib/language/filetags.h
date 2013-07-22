/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#ifndef QBS_FILETAGS_H
#define QBS_FILETAGS_H

#include <logging/logger.h>
#include <tools/id.h>
#include <QDataStream>
#include <QSet>

namespace qbs {
namespace Internal {

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
};

class FileTags : public QSet<FileTag>
{
public:
    QStringList toStringList() const;
    static FileTags fromStringList(const QStringList &strings);
    bool matches(const FileTags &other) const;
};

LogWriter operator <<(LogWriter w, const FileTags &tags);
QDataStream &operator >>(QDataStream &s, FileTags & tags);
QDataStream &operator <<(QDataStream &s, const FileTags &tags);

} // namespace Internal
} // namespace qbs

#endif // QBS_FILETAGS_H

