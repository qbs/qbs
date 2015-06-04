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
#ifndef QBS_SETUPPROJECTPARAMETERS_H
#define QBS_SETUPPROJECTPARAMETERS_H

#include "qbs_export.h"

#include <tools/error.h>

#include <QProcessEnvironment>
#include <QSharedDataPointer>
#include <QStringList>
#include <QVariantMap>

namespace qbs {

class Settings;

namespace Internal { class SetupProjectParametersPrivate; }

class QBS_EXPORT SetupProjectParameters
{
public:
    SetupProjectParameters();
    SetupProjectParameters(const SetupProjectParameters &other);
    ~SetupProjectParameters();

    SetupProjectParameters &operator=(const SetupProjectParameters &other);

    QString topLevelProfile() const;
    void setTopLevelProfile(const QString &profile);

    // TODO: It seems weird that we special-case the build variant. Can we get rid of this?
    QString buildVariant() const;
    void setBuildVariant(const QString &buildVariant);

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

    static QVariantMap expandedBuildConfiguration(const QString &settingsBaseDir,
            const QString &profileName, const QString &buildVariant, ErrorInfo *errorInfo = 0);
    ErrorInfo expandBuildConfiguration();
    QVariantMap buildConfiguration() const;
    QVariantMap buildConfigurationTree() const;

    static QVariantMap finalBuildConfigurationTree(const QVariantMap &buildConfig,
            const QVariantMap &overriddenValues, const QString &buildRoot,
            const QString &topLevelProfile);
    QVariantMap finalBuildConfigurationTree() const;

    bool ignoreDifferentProjectFilePath() const;
    void setIgnoreDifferentProjectFilePath(bool doIgnore);

    bool dryRun() const;
    void setDryRun(bool dryRun);

    bool logElapsedTime() const;
    void setLogElapsedTime(bool logElapsedTime);

    QProcessEnvironment environment() const;
    void setEnvironment(const QProcessEnvironment &env);
    QProcessEnvironment adjustedEnvironment() const;

    enum RestoreBehavior { RestoreOnly, ResolveOnly, RestoreAndTrackChanges };
    RestoreBehavior restoreBehavior() const;
    void setRestoreBehavior(RestoreBehavior behavior);

    enum PropertyCheckingMode { PropertyCheckingStrict, PropertyCheckingRelaxed };
    PropertyCheckingMode propertyCheckingMode() const;
    void setPropertyCheckingMode(PropertyCheckingMode mode);

private:
    QSharedDataPointer<Internal::SetupProjectParametersPrivate> d;
};

} // namespace qbs

#endif // Include guard
