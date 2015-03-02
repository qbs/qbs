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

#ifndef QBS_ERROR
#define QBS_ERROR

#include "codelocation.h"

#include <QExplicitlySharedDataPointer>
#include <QList>
#include <QMetaType>
#include <QSharedDataPointer>

QT_BEGIN_NAMESPACE
class QString;
QT_END_NAMESPACE

namespace qbs {
class CodeLocation;

class QBS_EXPORT ErrorItem
{
    friend class ErrorInfo;
public:
    ErrorItem();
    ErrorItem(const ErrorItem &rhs);
    ErrorItem &operator=(const ErrorItem &other);
    ~ErrorItem();

    QString description() const;
    CodeLocation codeLocation() const;
    QString toString() const;

private:
    ErrorItem(const QString &description, const CodeLocation &codeLocation);

    class ErrorItemPrivate;
    QExplicitlySharedDataPointer<ErrorItemPrivate> d;
};

class QBS_EXPORT ErrorInfo
{
public:
    ErrorInfo();
    ErrorInfo(const ErrorInfo &rhs);
    ErrorInfo(const QString &description, const CodeLocation &location = CodeLocation(),
              bool internalError = false);
    ErrorInfo &operator=(const ErrorInfo &other);
    ~ErrorInfo();

    void append(const QString &description, const CodeLocation &location = CodeLocation());
    void prepend(const QString &description, const CodeLocation &location = CodeLocation());
    QList<ErrorItem> items() const;
    bool hasError() const { return !items().isEmpty(); }
    void clear();
    QString toString() const;
    bool isInternalError() const;

private:
    class ErrorInfoPrivate;
    QSharedDataPointer<ErrorInfoPrivate> d;
};

} // namespace qbs

Q_DECLARE_METATYPE(qbs::ErrorInfo)

#endif // QBS_ERROR
