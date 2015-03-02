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

#ifndef QBS_QBSASSERT_H
#define QBS_QBSASSERT_H

#include "qbs_export.h"

// TODO: Remove once we require 5.3.
#ifndef Q_NORETURN
#define Q_NORETURN
#endif

namespace qbs {
namespace Internal {

QBS_EXPORT void writeAssertLocation(const char *condition, const char *file, int line);
QBS_EXPORT Q_NORETURN void throwAssertLocation(const char *condition, const char *file, int line);

} // namespace Internal
} // namespace qbs

#define QBS_ASSERT(cond, action)\
    if (Q_LIKELY(cond)) {} else {\
        ::qbs::Internal::writeAssertLocation(#cond, __FILE__, __LINE__); action;\
    } do {} while (0)

// The do {} while (0) is here to enforce the use of a semicolon after QBS_ASSERT.
// action can also be continue or break. Copied from qtcassert.h in Qt Creator.

#define QBS_CHECK(cond)\
    do {\
        if (Q_LIKELY(cond)) {} else {\
            ::qbs::Internal::throwAssertLocation(#cond, __FILE__, __LINE__);\
        }\
    } while (0)

#endif // QBS_QBSASSERT_H
