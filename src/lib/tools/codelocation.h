/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#ifndef QBS_SOURCELOCATION_H
#define QBS_SOURCELOCATION_H

#include <QDataStream>
#include <QString>

namespace qbs {

struct CodeLocation
{
    CodeLocation()
        : line(0), column(0)
    {}

    CodeLocation(const QString &aFileName, int aLine = 0, int aColumn = 0)
        : fileName(aFileName),
          line(aLine),
          column(aColumn)
    {}

    bool isValid() const
    {
        return !fileName.isEmpty();
    }

    bool operator != (const CodeLocation &rhs) const
    {
        return fileName != rhs.fileName || line != rhs.line || column != rhs.column;
    }

    QString fileName;
    int line;
    int column;
};

} // namespace qbs

QT_BEGIN_NAMESPACE
inline QDataStream &operator<< (QDataStream &s, const qbs::CodeLocation &o)
{
    s << o.fileName;
    s << o.line;
    s << o.column;
    return s;
}

inline QDataStream &operator>> (QDataStream &s, qbs::CodeLocation &o)
{
    s >> o.fileName;
    s >> o.line;
    s >> o.column;
    return s;
}
QT_END_NAMESPACE

#endif // QBS_SOURCELOCATION_H
