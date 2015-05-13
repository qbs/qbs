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

#ifndef QBS_SOURCELOCATION_H
#define QBS_SOURCELOCATION_H

#include "qbs_export.h"

#include <QDebug>
#include <QExplicitlySharedDataPointer>

QT_BEGIN_NAMESPACE
class QDataStream;
class QString;
QT_END_NAMESPACE

namespace qbs {
namespace Internal { class PersistentPool; }

class QBS_EXPORT CodeLocation
{
    friend QBS_EXPORT bool operator==(const CodeLocation &cl1, const CodeLocation &cl2);
public:
    CodeLocation();
    CodeLocation(const QString &aFilePath, int aLine = -1, int aColumn = -1, bool checkPath = true);
    CodeLocation(const CodeLocation &other);
    CodeLocation &operator=(const CodeLocation &other);
    ~CodeLocation();

    QString filePath() const;
    int line() const;
    int column() const;

    bool isValid() const;
    QString toString() const;

    void load(Internal::PersistentPool &pool);
    void store(Internal::PersistentPool &pool) const;

private:
    class CodeLocationPrivate;
    QExplicitlySharedDataPointer<CodeLocationPrivate> d;
};

QBS_EXPORT bool operator==(const CodeLocation &cl1, const CodeLocation &cl2);
QBS_EXPORT bool operator!=(const CodeLocation &cl1, const CodeLocation &cl2);

QDebug operator<<(QDebug debug, const CodeLocation &location);

} // namespace qbs

#endif // QBS_SOURCELOCATION_H
