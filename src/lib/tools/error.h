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

#ifndef QBS_ERROR
#define QBS_ERROR

#include "codelocation.h"

namespace qbs {

class ErrorData
{
public:
    ErrorData();
    ErrorData(const QString &description, const QString &file = QString(),
              int line = 0, int column = 0);
    ErrorData(const ErrorData &rhs);

    const QString &description() const { return m_description; }
    const QString &file() const { return m_file; }
    int line() const { return m_line; }
    int column() const { return m_column; }
    QString toString() const;

private:
    QString m_description;
    QString m_file;
    int m_line;
    int m_column;
};

class Error
{
public:
    Error();
    Error(const Error &rhs);
    Error(const QString &description,
          const QString &file = QString(),
          int line = 0, int column = 0);
    Error(const QString &description, const CodeLocation &location);

    void append(const QString &description,
                const QString &file = QString(),
                int line = 0, int column = 0);
    void append(const QString &description, const CodeLocation &location);
    const QList<ErrorData> &entries() const;
    void clear();
    QString toString() const;

private:
    QList<ErrorData> m_data;
};

} // namespace qbs

#define QBS_TESTLIB_CATCH catch (qbs::Error &e) { \
    QFAIL(qPrintable(QString(QLatin1String("\n%1\n")).arg(e.toString())));}

#endif // QBS_ERROR
