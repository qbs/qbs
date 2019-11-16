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
#ifndef QBS_SETUPPROJECTPARAMETERS_H
#define QBS_SETUPPROJECTPARAMETERS_H

#include "qbs_export.h"

#include <tools/error.h>

#include <QtCore/qshareddata.h>

QT_BEGIN_NAMESPACE
class QProcessEnvironment;
class QStringList;
using QVariantMap = QMap<QString, QVariant>;
QT_END_NAMESPACE

namespace qbs {

class Profile;
class Settings;

namespace Internal { class SetupProjectParametersPrivate; }

enum class ErrorHandlingMode { Strict, Relaxed };

class QBS_EXPORT SetupProjectParameters
{
public:
    SetupProjectParameters();
    SetupProjectParameters(const SetupProjectParameters &other);
    SetupProjectParameters(SetupProjectParameters &&other) Q_DECL_NOEXCEPT;
    ~SetupProjectParameters();

    SetupProjectParameters &operator=(const SetupProjectParameters &other);
    SetupProjectParameters &operator=(SetupProjectParameters &&other) Q_DECL_NOEXCEPT;

    static SetupProjectParameters fromJson(const QJsonObject &data);

    QString topLevelProfile() const;
    void setTopLevelProfile(const QString &profile);

    QString configurationName() const;
    void setConfigurationName(const QString &configurationName);

    QString projectFilePath() const;
    void setProjectFilePath(const QString &projectFilePath);

    QString buildRoot() const;
    void setBuildRoot(const QString &buildRoot);

    QStringList searchPaths() const;
    void setSearchPaths(const QStringList &searchPaths);

    QStringList pluginPaths() const;
    void setPluginPaths(const QStringList &pluginPaths);

    QString libexecPath() const;
    void setLibexecPath(const QString &libexecPath);

    QString settingsDirectory() const;
    void setSettingsDirectory(const QString &settingsBaseDir);

    QVariantMap overriddenValues() const;
    void setOverriddenValues(const QVariantMap &values);
    QVariantMap overriddenValuesTree() const;

    static QVariantMap expandedBuildConfiguration(const Profile &profile,
                                                  const QString &configurationName,
                                                  ErrorInfo *errorInfo = nullptr);
    ErrorInfo expandBuildConfiguration();
    QVariantMap buildConfiguration() const;
    QVariantMap buildConfigurationTree() const;

    static QVariantMap finalBuildConfigurationTree(const QVariantMap &buildConfig,
            const QVariantMap &overriddenValues);
    QVariantMap finalBuildConfigurationTree() const;

    bool overrideBuildGraphData() const;
    void setOverrideBuildGraphData(bool doOverride);

    bool dryRun() const;
    void setDryRun(bool dryRun);

    bool logElapsedTime() const;
    void setLogElapsedTime(bool logElapsedTime);

    bool forceProbeExecution() const;
    void setForceProbeExecution(bool force);

    bool waitLockBuildGraph() const;
    void setWaitLockBuildGraph(bool wait);

    bool fallbackProviderEnabled() const;
    void setFallbackProviderEnabled(bool enable);

    QProcessEnvironment environment() const;
    void setEnvironment(const QProcessEnvironment &env);
    QProcessEnvironment adjustedEnvironment() const;

    enum RestoreBehavior { RestoreOnly, ResolveOnly, RestoreAndTrackChanges };
    RestoreBehavior restoreBehavior() const;
    void setRestoreBehavior(RestoreBehavior behavior);

    ErrorHandlingMode propertyCheckingMode() const;
    void setPropertyCheckingMode(ErrorHandlingMode mode);

    ErrorHandlingMode productErrorMode() const;
    void setProductErrorMode(ErrorHandlingMode mode);

private:
    QSharedDataPointer<Internal::SetupProjectParametersPrivate> d;
};

} // namespace qbs

#endif // Include guard
