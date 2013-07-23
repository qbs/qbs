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
#ifndef QBS_BUILDOPTIONS_H
#define QBS_BUILDOPTIONS_H

#include "qbs_export.h"

#include <QSharedDataPointer>
#include <QStringList>

namespace qbs {
namespace Internal { class BuildOptionsPrivate; }

class QBS_EXPORT BuildOptions
{
public:
    BuildOptions();
    BuildOptions(const BuildOptions &other);
    BuildOptions &operator=(const BuildOptions &other);
    ~BuildOptions();

    QStringList changedFiles() const;
    void setChangedFiles(const QStringList &changedFiles);

    QStringList activeFileTags() const;
    void setActiveFileTags(const QStringList &fileTags);

    static int defaultMaxJobCount();
    int maxJobCount() const;
    void setMaxJobCount(int jobCount);

    bool dryRun() const;
    void setDryRun(bool dryRun);

    bool keepGoing() const;
    void setKeepGoing(bool keepGoing);

    bool forceTimestampCheck() const;
    void setForceTimestampCheck(bool enabled);

    bool logElapsedTime() const;
    void setLogElapsedTime(bool log);

private:
    QSharedDataPointer<Internal::BuildOptionsPrivate> d;
};

bool operator==(const BuildOptions &bo1, const BuildOptions &bo2);
inline bool operator!=(const BuildOptions &bo1, const BuildOptions &bo2) { return !(bo1 == bo2); }

} // namespace qbs

#endif // QBS_BUILDOPTIONS_H
