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

#ifndef QBS_ERR
#define QBS_ERR

#include "codelocation.h"

namespace qbs {

class ErrorData
{
public:
    ErrorData() :
        line(0),
        column(0)
    { }

    ErrorData(const QString &_description, const QString &_file = QString(),
              int _line = 0, int _column = 0)
        : description(_description)
        , file(_file)
        , line(_line)
        , column(_column)
    { }

    ErrorData(const ErrorData &rhs)
        : description(rhs.description)
        , file(rhs.file)
        , line(rhs.line)
        , column(rhs.column)
    { }

    QString toString() const;

    QString description;
    QString file;
    int line, column;
};

class Error
{
public:
    Error()
    { }

    Error(const Error &rhs)
        : data(rhs.data)
    { }

    Error(const QString &_description,
          const QString &_file = QString(),
          int _line = 0, int _column = 0)
    {
        append(_description, _file, _line, _column);
    }

    Error(const QString &_description, const CodeLocation &location)
    {
        append(_description, location.fileName, location.line, location.column);
    }

    void append(const QString &_description,
                const QString &_file = QString(),
                int _line = 0, int _column = 0);
    void append(const QString &_description, const CodeLocation &location);

    QString toString() const;
    void clear();

    QList<ErrorData> data;
};

} // namespace qbs

#define QBS_TESTLIB_CATCH catch (qbs::Error &e) { \
    QFAIL(qPrintable(QString(QLatin1String("\n%1\n")).arg(e.toString())));}

#endif
