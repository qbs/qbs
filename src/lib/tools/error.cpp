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

#include "error.h"

#include <QSharedData>
#include <QStringList>

namespace qbs {

class ErrorItem::ErrorItemPrivate : public QSharedData
{
public:
    QString description;
    CodeLocation codeLocation;
};

/*!
 * \class ErrorData
 * \brief The \c ErrorData class describes (part of) an error resulting from a qbs operation.
 * It is always delivered as part of an \c Error.
 * \sa Error
 */

ErrorItem::ErrorItem() : d(new ErrorItemPrivate)
{
}

ErrorItem::ErrorItem(const QString &description, const CodeLocation &codeLocation)
    : d(new ErrorItemPrivate)
{
    d->description = description;
    d->codeLocation = codeLocation;
}

ErrorItem::ErrorItem(const ErrorItem &rhs) : d(rhs.d)
{
}

ErrorItem &ErrorItem::operator=(const ErrorItem &other)
{
    d = other.d;
    return *this;
}

ErrorItem::~ErrorItem()
{
}

QString ErrorItem::description() const
{
    return d->description;
}

CodeLocation ErrorItem::codeLocation() const
{
    return d->codeLocation;
}

/*!
 * \fn const QString &ErrorData::description() const
 * \brief A general description of the error.
 */

 /*!
  * \fn const QString &ErrorData::codeLocation() const
  * \brief The location at which file in which the error occurred.
  * \note This information might not be applicable, in which case location().isValid() returns false
  */

/*!
 * \brief A full textual description of the error using all available information.
 */
QString ErrorItem::toString() const
{
    QString str = codeLocation().toString();
    if (!str.isEmpty())
        str += QLatin1Char(' ');
    return str += description();
}


class ErrorInfo::ErrorInfoPrivate : public QSharedData
{
public:
    QList<ErrorItem> items;
};

/*!
 * \class Error
 * \brief Represents an error resulting from a qbs operation.
 * It is made up of one or more \c ErrorData objects.
 * \sa ErrorData
 */

ErrorInfo::ErrorInfo() : d(new ErrorInfoPrivate)
{
}

ErrorInfo::ErrorInfo(const ErrorInfo &rhs) : d(rhs.d)
{
}

ErrorInfo::ErrorInfo(const QString &description, const CodeLocation &location) : d(new ErrorInfoPrivate)
{
    append(description, location);
}

ErrorInfo &ErrorInfo::operator =(const ErrorInfo &other)
{
    d = other.d;
    return *this;
}

ErrorInfo::~ErrorInfo()
{
}

void ErrorInfo::append(const QString &description, const CodeLocation &location)
{
    d->items.append(ErrorItem(description, location));
}

void ErrorInfo::prepend(const QString &description, const CodeLocation &location)
{
    d->items.prepend(ErrorItem(description, location));
}

/*!
 * \brief A list of concrete error description.
 * Most often, there will be one element in this list, but there can be more e.g. to illustrate
 * how an error condition propagates through several source files.
 */
QList<ErrorItem> ErrorInfo::items() const
{
    return d->items;
}

void ErrorInfo::clear()
{
    d->items.clear();
}

/*!
 * \brief A complete textual description of the error.
 * All "sub-errors" will be represented.
 * \sa Error::entries()
 */
QString ErrorInfo::toString() const
{
    QStringList lines;
    foreach (const ErrorItem &e, d->items)
        lines.append(e.toString());
    return lines.join(QLatin1String("\n"));
}

} // namespace qbs
