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

#include "joblimits.h"

#include <tools/persistence.h>

#include <utility>
#include <vector>

namespace qbs {
namespace Internal {

static int transformLimit(int limitFromUser)
{
    return limitFromUser == 0
            ? std::numeric_limits<int>::max()
            : limitFromUser < -1 ? -1
            : limitFromUser;
}

class JobLimitPrivate : public QSharedData
{
public:
    JobLimitPrivate(const QString &pool, int limit)
        : jobLimit(std::make_pair(pool, transformLimit(limit)))
    {
    }
    template<PersistentPool::OpType opType> void serializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(jobLimit);
    }
    std::pair<QString, int> jobLimit;
};

class JobLimitsPrivate : public QSharedData
{
public:
    template<PersistentPool::OpType opType> void serializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(jobLimits);
    }
    std::vector<JobLimit> jobLimits;
};

} // namespace Internal

JobLimit::JobLimit() : JobLimit(QString(), -1)
{
}
JobLimit::JobLimit(const QString &pool, int limit) : d(new Internal::JobLimitPrivate(pool, limit))
{
}
JobLimit::JobLimit(const JobLimit &other) = default;
JobLimit &JobLimit::operator=(const JobLimit &other) = default;
JobLimit::~JobLimit() = default;
QString JobLimit::pool() const { return d->jobLimit.first; }
int JobLimit::limit() const { return d->jobLimit.second; }

void JobLimit::load(Internal::PersistentPool &pool)
{
    d->serializationOp<Internal::PersistentPool::Load>(pool);
}

void JobLimit::store(Internal::PersistentPool &pool)
{
    d->serializationOp<Internal::PersistentPool::Store>(pool);
}

JobLimits::JobLimits() : d(new Internal::JobLimitsPrivate) { }
JobLimits::JobLimits(const JobLimits &other) = default;
JobLimits &JobLimits::operator=(const JobLimits &other) = default;
JobLimits::~JobLimits() = default;

void JobLimits::setJobLimit(const JobLimit &limit)
{
    for (auto &currentLimit : d->jobLimits) {
        if (currentLimit.pool() == limit.pool()) {
            if (currentLimit.limit() != limit.limit())
                currentLimit = limit;
            return;
        }
    }
    d->jobLimits.push_back(limit);
}

void JobLimits::setJobLimit(const QString &pool, int limit)
{
    setJobLimit(JobLimit(pool, limit));
}

int JobLimits::getLimit(const QString &pool) const
{
    for (const JobLimit &l : d->jobLimits) {
        if (l.pool() == pool)
            return l.limit();
    }
    return -1;
}

bool JobLimits::isEmpty() const
{
    return d->jobLimits.empty();
}

int JobLimits::count() const
{
    return int(d->jobLimits.size());
}

JobLimit JobLimits::jobLimitAt(int i) const
{
    return d->jobLimits.at(i);
}

JobLimits &JobLimits::update(const JobLimits &other)
{
    if (isEmpty()) {
        *this = other;
    } else {
        for (int i = 0; i < other.count(); ++i) {
            const JobLimit &l = other.jobLimitAt(i);
            if (l.limit() != -1)
                setJobLimit(l);
        }
    }
    return *this;
}

void JobLimits::load(Internal::PersistentPool &pool)
{
    d->serializationOp<Internal::PersistentPool::Load>(pool);
}

void JobLimits::store(Internal::PersistentPool &pool)
{
    d->serializationOp<Internal::PersistentPool::Store>(pool);
}

} // namespace qbs
