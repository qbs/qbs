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

#ifndef QBS_SOURCELOCATION_H
#define QBS_SOURCELOCATION_H

#include "qbs_export.h"

#include <QExplicitlySharedDataPointer>

QT_BEGIN_NAMESPACE
class QDataStream;
class QString;
QT_END_NAMESPACE

namespace qbs {

class QBS_EXPORT CodeLocation
{
public:
    CodeLocation();
    CodeLocation(const QString &aFileName, int aLine = -1, int aColumn = -1);
    CodeLocation(const CodeLocation &other);
    CodeLocation &operator=(const CodeLocation &other);
    ~CodeLocation();

    QString fileName() const;
    int line() const;
    int column() const;

    bool isValid() const;
    QString toString() const;
private:
    class CodeLocationPrivate;
    QExplicitlySharedDataPointer<CodeLocationPrivate> d;
};

QBS_EXPORT bool operator==(const CodeLocation &cl1, const CodeLocation &cl2);
QBS_EXPORT bool operator!=(const CodeLocation &cl1, const CodeLocation &cl2);

QDataStream &operator<<(QDataStream &s, const CodeLocation &o);
QDataStream &operator>>(QDataStream &s, CodeLocation &o);

} // namespace qbs

#endif // QBS_SOURCELOCATION_H
