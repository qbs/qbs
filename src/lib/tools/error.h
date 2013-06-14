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
    ErrorInfo(const QString &description, const CodeLocation &location = CodeLocation());
    ErrorInfo &operator=(const ErrorInfo &other);
    ~ErrorInfo();

    void append(const QString &description, const CodeLocation &location = CodeLocation());
    void prepend(const QString &description, const CodeLocation &location = CodeLocation());
    QList<ErrorItem> items() const;
    bool hasError() const { return !items().isEmpty(); }
    void clear();
    QString toString() const;

private:
    class ErrorInfoPrivate;
    QSharedDataPointer<ErrorInfoPrivate> d;
};

} // namespace qbs

Q_DECLARE_METATYPE(qbs::ErrorInfo)

#endif // QBS_ERROR
