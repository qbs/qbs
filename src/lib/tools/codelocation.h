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

#ifndef SOURCELOCATION_H
#define SOURCELOCATION_H

#include <QtCore/QString>
#include <QtCore/QDataStream>

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

#endif // SOURCELOCATION_H
