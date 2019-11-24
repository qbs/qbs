/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef QBS_BENCHMARKER_EXCEPTION_H
#define QBS_BENCHMARKER_EXCEPTION_H

#include <QtCore/qexception.h>
#include <QtCore/qstring.h>

namespace qbsBenchmarker {

class Exception : public QException {
public:
    explicit Exception(QString description) : m_description(std::move(description)) {}
    ~Exception() throw() override = default;

    QString description() const { return m_description; }

private:
    void raise() const override { throw *this; }
    Exception *clone() const override { return new Exception(*this); }

    QString m_description;
};

} // namespace qbsBenchmarker

#endif // Include guard.

