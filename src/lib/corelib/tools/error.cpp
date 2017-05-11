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

#include "error.h"

#include "persistence.h"
#include "qttools.h"

#include <QtCore/qshareddata.h>
#include <QtCore/qstringlist.h>

#include <algorithm>
#include <functional>
#include <regex>

namespace qbs {

class ErrorItem::ErrorItemPrivate : public QSharedData
{
public:
    QString description;
    CodeLocation codeLocation;
    bool isBacktraceItem = false;
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

ErrorItem::ErrorItem(const QString &description, const CodeLocation &codeLocation,
                     bool isBacktraceItem)
    : d(new ErrorItemPrivate)
{
    d->description = description;
    d->codeLocation = codeLocation;
    d->isBacktraceItem = isBacktraceItem;
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

bool ErrorItem::isBacktraceItem() const
{
    return d->isBacktraceItem;
}

void ErrorItem::load(Internal::PersistentPool &pool)
{
    pool.load(d->description);
    pool.load(d->codeLocation);
    pool.load(d->isBacktraceItem);
}

void ErrorItem::store(Internal::PersistentPool &pool) const
{
    pool.store(d->description);
    pool.store(d->codeLocation);
    pool.store(d->isBacktraceItem);
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
    ErrorInfoPrivate() : internalError(false) { }

    QList<ErrorItem> items;
    bool internalError;
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

ErrorInfo::ErrorInfo(const QString &description, const CodeLocation &location, bool internalError)
    : d(new ErrorInfoPrivate)
{
    append(description, location);
    d->internalError = internalError;
}

ErrorInfo::ErrorInfo(const QString &description, const QStringList &backtrace)
    : d(new ErrorInfoPrivate)
{
    append(description);
    for (const QString &traceLine : backtrace) {
        static const std::regex regexp("^(.+) at (.+):(\\-?[0-9]+)$");
        std::smatch match;
        const std::string tl = traceLine.toStdString();
        if (std::regex_match(tl, match, regexp)) {
            const QString message = QString::fromStdString(match[1]),
                             file = QString::fromStdString(match[2]),
                             line = QString::fromStdString(match[3]);
            const CodeLocation location(file, line.toInt());
            appendBacktrace(message, location);
        }
    }
}


ErrorInfo &ErrorInfo::operator =(const ErrorInfo &other)
{
    d = other.d;
    return *this;
}

ErrorInfo::~ErrorInfo()
{
}

void ErrorInfo::appendBacktrace(const QString &description, const CodeLocation &location)
{
    d->items.append(ErrorItem(description, location, true));
}

void ErrorInfo::append(const ErrorItem &item)
{
    d->items.append(item);
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
    for (const ErrorItem &e : qAsConst(d->items)) {
        if (e.isBacktraceItem()) {
            QString line;
            if (!e.description().isEmpty())
                line.append(QStringLiteral(" at %1").arg(e.description()));
            if (e.codeLocation().isValid())
                line.append(QStringLiteral(" in %1").arg(e.codeLocation().toString()));
            if (!line.isEmpty())
                lines.append(QStringLiteral("\t") + line);
        } else {
            lines.append(e.toString());
        }
    }
    return lines.join(QLatin1Char('\n'));
}

/*!
 * \brief Returns true if this error represents a bug in qbs, false otherwise.
 */
bool ErrorInfo::isInternalError() const
{
    return d->internalError;
}

void ErrorInfo::load(Internal::PersistentPool &pool)
{
    pool.load(d->items);
    pool.load(d->internalError);
}

void ErrorInfo::store(Internal::PersistentPool &pool) const
{
    pool.store(d->items);
    pool.store(d->internalError);
}

void appendError(ErrorInfo &dst, const ErrorInfo &src)
{
    const QList<ErrorItem> &sourceItems = src.items();
    for (const ErrorItem &item : sourceItems)
        dst.append(item);
}

} // namespace qbs
