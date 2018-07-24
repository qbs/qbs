/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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
#ifndef QBS_JOB_LIMITS_H
#define QBS_JOB_LIMITS_H

#include "qbs_export.h"

#include <QtCore/qshareddata.h>

QT_BEGIN_NAMESPACE
class QString;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {
class JobLimitPrivate;
class JobLimitsPrivate;
class PersistentPool;
}

class QBS_EXPORT JobLimit
{
public:
    JobLimit();
    JobLimit(const QString &pool, int limit);
    JobLimit(const JobLimit &other);
    JobLimit &operator=(const JobLimit &other);
    ~JobLimit();

    QString pool() const;
    int limit() const;

    void load(Internal::PersistentPool &pool);
    void store(Internal::PersistentPool &pool);
private:
    QSharedDataPointer<Internal::JobLimitPrivate> d;
};

class QBS_EXPORT JobLimits
{
public:
    JobLimits();
    JobLimits(const JobLimits &other);
    JobLimits &operator=(const JobLimits &other);
    ~JobLimits();

    void setJobLimit(const JobLimit &limit);
    void setJobLimit(const QString &pool, int limit);
    int getLimit(const QString &pool) const;
    bool hasLimit(const QString &pool) const { return getLimit(pool) != -1; }
    bool isEmpty() const;

    int count() const;
    JobLimit jobLimitAt(int i) const;

    JobLimits &update(const JobLimits &other);

    void load(Internal::PersistentPool &pool);
    void store(Internal::PersistentPool &pool);
private:
    QSharedDataPointer<Internal::JobLimitsPrivate> d;
};

} // namespace qbs

#endif // include guard
