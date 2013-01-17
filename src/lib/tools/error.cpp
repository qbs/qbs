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

#include <QDir>

namespace qbs {

/*!
 * \class ErrorData
 * \brief The \c ErrorData class describes (part of) an error resulting from a qbs operation.
 * It is always delivered as part of an \c Error.
 * \sa Error
 */

ErrorData::ErrorData()
{
}

ErrorData::ErrorData(const QString &description, const CodeLocation &codeLocation)
    : m_description(description)
    , m_codeLocation(codeLocation)
{
}

ErrorData::ErrorData(const ErrorData &rhs)
    : m_description(rhs.m_description)
    , m_codeLocation(rhs.m_codeLocation)
{
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
QString ErrorData::toString() const
{
    QString str;
    if (!m_codeLocation.fileName.isEmpty()) {
        str = QDir::toNativeSeparators(m_codeLocation.fileName);
        QString lineAndColumn;
        if (m_codeLocation.line > 0 && !str.contains(QRegExp(QLatin1String(":[0-9]+$"))))
            lineAndColumn += QLatin1Char(':') + QString::number(m_codeLocation.line);
        if (m_codeLocation.column > 0 && !str.contains(QRegExp(QLatin1String(":[0-9]+:[0-9]+$"))))
            lineAndColumn += QLatin1Char(':') + QString::number(m_codeLocation.column);
        str += lineAndColumn;
        str += QLatin1Char(' ') + m_description;
    } else {
        str = m_description;
    }
    return str;
}

/*!
 * \class Error
 * \brief Represents an error resulting from a qbs operation.
 * It is made up of one or more \c ErrorData objects.
 * \sa ErrorData
 */

Error::Error()
{
}

Error::Error(const Error &rhs)
    : m_data(rhs.m_data)
{
}

Error::Error(const QString &description, const CodeLocation &location)
{
    append(description, location);
}

void Error::append(const QString &description, const CodeLocation &location)
{
    m_data.append(ErrorData(description, location));
}

void Error::prepend(const QString &description, const CodeLocation &location)
{
    m_data.prepend(ErrorData(description, location));
}

/*!
 * \brief A list of concrete error description.
 * Most often, there will be one element in this list, but there can be more e.g. to illustrate
 * how an error condition propagates through several source files.
 */
const QList<ErrorData> &Error::entries() const
{
    return m_data;
}

void Error::clear()
{
    m_data.clear();
}

/*!
 * \brief A complete textual description of the error.
 * All "sub-errors" will be represented.
 * \sa Error::entries()
 */
QString Error::toString() const
{
    QStringList lines;
    foreach (const ErrorData &e, m_data)
        lines.append(e.toString());
    return lines.join(QLatin1String("\n"));
}

} // namespace qbs
