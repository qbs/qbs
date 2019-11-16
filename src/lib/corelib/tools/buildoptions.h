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
#ifndef QBS_BUILDOPTIONS_H
#define QBS_BUILDOPTIONS_H

#include "qbs_export.h"

#include "commandechomode.h"
#include "joblimits.h"

#include <QtCore/qshareddata.h>

QT_BEGIN_NAMESPACE
class QJsonObject;
class QStringList;
QT_END_NAMESPACE

namespace qbs {
namespace Internal { class BuildOptionsPrivate; }

class QBS_EXPORT BuildOptions
{
public:
    BuildOptions();
    BuildOptions(const BuildOptions &other);
    BuildOptions &operator=(const BuildOptions &other);
    ~BuildOptions();

    static BuildOptions fromJson(const QJsonObject &data);

    QStringList filesToConsider() const;
    void setFilesToConsider(const QStringList &files);

    QStringList changedFiles() const;
    void setChangedFiles(const QStringList &changedFiles);

    QStringList activeFileTags() const;
    void setActiveFileTags(const QStringList &fileTags);

    static int defaultMaxJobCount();
    int maxJobCount() const;
    void setMaxJobCount(int jobCount);

    QString settingsDirectory() const;
    void setSettingsDirectory(const QString &settingsBaseDir);

    JobLimits jobLimits() const;
    void setJobLimits(const JobLimits &jobLimits);

    bool projectJobLimitsTakePrecedence() const;
    void setProjectJobLimitsTakePrecedence(bool toggle);

    bool dryRun() const;
    void setDryRun(bool dryRun);

    bool keepGoing() const;
    void setKeepGoing(bool keepGoing);

    bool forceTimestampCheck() const;
    void setForceTimestampCheck(bool enabled);

    bool forceOutputCheck() const;
    void setForceOutputCheck(bool enabled);

    bool logElapsedTime() const;
    void setLogElapsedTime(bool log);

    CommandEchoMode echoMode() const;
    void setEchoMode(CommandEchoMode echoMode);

    bool install() const;
    void setInstall(bool install);

    bool removeExistingInstallation() const;
    void setRemoveExistingInstallation(bool removeExisting);

    bool executeRulesOnly() const;
    void setExecuteRulesOnly(bool onlyRules);

private:
    QSharedDataPointer<Internal::BuildOptionsPrivate> d;
};

bool operator==(const BuildOptions &bo1, const BuildOptions &bo2);
inline bool operator!=(const BuildOptions &bo1, const BuildOptions &bo2) { return !(bo1 == bo2); }

} // namespace qbs

#endif // QBS_BUILDOPTIONS_H
