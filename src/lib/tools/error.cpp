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

#include "error.h"

#include <QDir>

namespace qbs {

ErrorData::ErrorData()
    : m_line(0)
    , m_column(0)
{
}

ErrorData::ErrorData(const QString &description, const QString &file, int line, int column)
    : m_description(description)
    , m_file(file)
    , m_line(line)
    , m_column(column)
{
}

ErrorData::ErrorData(const ErrorData &rhs)
    : m_description(rhs.m_description)
    , m_file(rhs.m_file)
    , m_line(rhs.m_line)
    , m_column(rhs.m_column)
{
}

QString ErrorData::toString() const
{
    QString str;
    if (!m_file.isEmpty()) {
        str = QDir::toNativeSeparators(m_file);
        QString lineAndColumn;
        if (m_line > 0 && !str.contains(QRegExp(QLatin1String(":[0-9]+$"))))
            lineAndColumn += QLatin1Char(':') + QString::number(m_line);
        if (m_column > 0 && !str.contains(QRegExp(QLatin1String(":[0-9]+:[0-9]+$"))))
            lineAndColumn += QLatin1Char(':') + QString::number(m_column);
        str += lineAndColumn;
        str += QLatin1Char(' ') + m_description;
    } else {
        str = m_description;
    }
    return str;
}

Error::Error()
{
}

Error::Error(const Error &rhs)
    : m_data(rhs.m_data)
{
}

Error::Error(const QString &description, const QString &file, int line, int column)
{
    append(description, file, line, column);
}

Error::Error(const QString &description, const CodeLocation &location)
{
    append(description, location.fileName, location.line, location.column);
}

void Error::append(const QString &description, const QString &file, int line, int column)
{
    m_data.append(ErrorData(description, file, line, column));
}

void Error::append(const QString &description, const CodeLocation &location)
{
    m_data.append(ErrorData(description, location.fileName, location.line, location.column));
}

const QList<ErrorData> &Error::entries() const
{
    return m_data;
}

void Error::clear()
{
    m_data.clear();
}

QString Error::toString() const
{
    QStringList lines;
    foreach (const ErrorData &e, m_data)
        lines.append(e.toString());
    return lines.join(QLatin1String("\n"));
}

} // namespace qbs
