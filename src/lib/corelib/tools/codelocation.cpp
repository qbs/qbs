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
#include "codelocation.h"

#include <tools/fileinfo.h>
#include <tools/persistence.h>
#include <tools/qbsassert.h>
#include <tools/stringconstants.h>

#include <QtCore/qdatastream.h>
#include <QtCore/qdir.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>

namespace qbs {

CodeLocation::CodeLocation(const QString &aFilePath, int aLine, int aColumn, bool checkPath)
{
    QBS_ASSERT(!checkPath || Internal::FileInfo::isAbsolute(aFilePath), qDebug() << aFilePath);
    m_filePath = aFilePath;
    m_line = aLine;
    m_column = aColumn;
}

QString CodeLocation::toString() const
{
    QString str;
    if (isValid()) {
        str = QDir::toNativeSeparators(filePath());
        QString lineAndColumn;
        if (line() > 0 && !str.contains(QRegularExpression(QStringLiteral(":[0-9]+$"))))
            lineAndColumn += QLatin1Char(':') + QString::number(line());
        if (column() > 0 && !str.contains(QRegularExpression(QStringLiteral(":[0-9]+:[0-9]+$"))))
            lineAndColumn += QLatin1Char(':') + QString::number(column());
        str += lineAndColumn;
    }
    return str;
}

QJsonObject CodeLocation::toJson() const
{
    QJsonObject obj;
    if (!filePath().isEmpty())
        obj.insert(Internal::StringConstants::filePathKey(), filePath());
    if (line() != -1)
        obj.insert(QStringLiteral("line"), line());
    if (column() != -1)
        obj.insert(QStringLiteral("column"), column());
    return obj;
}

void CodeLocation::load(Internal::PersistentPool &pool)
{
    const bool isValid = pool.load<bool>();
    if (!isValid)
        return;
    pool.load(m_filePath);
    pool.load(m_line);
    pool.load(m_column);
}

void CodeLocation::store(Internal::PersistentPool &pool) const
{
    if (isValid()) {
        pool.store(true);
        pool.store(m_filePath);
        pool.store(m_line);
        pool.store(m_column);
    } else {
        pool.store(false);
    }
}

bool operator==(const CodeLocation &cl1, const CodeLocation &cl2)
{
    return cl1.filePath() == cl2.filePath() && cl1.line() == cl2.line()
           && cl1.column() == cl2.column();
}

bool operator!=(const CodeLocation &cl1, const CodeLocation &cl2)
{
    return !(cl1 == cl2);
}

QDebug operator<<(QDebug debug, const CodeLocation &location)
{
    return debug << location.toString();
}

bool operator<(const CodeLocation &cl1, const CodeLocation &cl2)
{
    return cl1.toString() < cl2.toString();
}

void CodePosition::load(Internal::PersistentPool &pool) { pool.load(m_line, m_column); }
void CodePosition::store(Internal::PersistentPool &pool) const { pool.store(m_line, m_column); }

bool operator==(const CodePosition &pos1, const CodePosition &pos2)
{
    return pos1.line() == pos2.line() && pos1.column() == pos2.column();
}
bool operator!=(const CodePosition &pos1, const CodePosition &pos2) { return !(pos1 == pos2); }

bool operator<(const CodePosition &pos1, const CodePosition &pos2)
{
    const int lineDiff = pos1.line() - pos2.line();
    if (lineDiff < 0)
        return true;
    if (lineDiff > 0)
        return false;
    return pos1.column() < pos2.column();
}
bool operator>(const CodePosition &pos1, const CodePosition &pos2) { return pos2 < pos1; }
bool operator<=(const CodePosition &pos1, const CodePosition &pos2) { return !(pos1 > pos2); }
bool operator>=(const CodePosition &pos1, const CodePosition &pos2) { return !(pos1 < pos2); }

CodeRange::CodeRange(const CodePosition &start, const CodePosition &end)
    : m_start(start), m_end(end) {}

void CodeRange::load(Internal::PersistentPool &pool) { pool.load(m_start, m_end); }
void CodeRange::store(Internal::PersistentPool &pool) const { pool.store(m_start, m_end); }

bool CodeRange::contains(const CodePosition &pos) const
{
    return start() <= pos && end() > pos;
}

bool operator==(const CodeRange &r1, const CodeRange &r2)
{
    return r1.start() == r2.start() && r1.end() == r2.end();
}
bool operator!=(const CodeRange &r1, const CodeRange &r2) { return !(r1 == r2); }
bool operator<(const CodeRange &r1, const CodeRange &r2) { return r1.start() < r2.start(); }

} // namespace qbs
