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

    QString projectFilePath() const;
    void setProjectFilePath(const QString &projectFilePath);

    QString buildRoot() const;
    void setBuildRoot(const QString &buildRoot);

    QStringList searchPaths() const;
    void setSearchPaths(const QStringList &searchPaths);

    QStringList pluginPaths() const;
    void setPluginPaths(const QStringList &pluginPaths);

    QVariantMap overriddenValues() const;
    void setOverriddenValues(const QVariantMap &values);
    QVariantMap overriddenValuesTree() const;

    QVariantMap buildConfiguration() const;
    void setBuildConfiguration(const QVariantMap &buildConfiguration);
    QVariantMap buildConfigurationTree() const;
    ErrorInfo expandBuildConfiguration(Settings *settings);

    bool ignoreDifferentProjectFilePath() const;
    void setIgnoreDifferentProjectFilePath(bool doIgnore);

    bool dryRun() const;
    void setDryRun(bool dryRun);

    bool logElapsedTime() const;
    void setLogElapsedTime(bool logElapsedTime);

    QProcessEnvironment environment() const;
    void setEnvironment(const QProcessEnvironment &env);

    enum RestoreBehavior { RestoreOnly, ResolveOnly, RestoreAndTrackChanges };
    RestoreBehavior restoreBehavior() const;
    void setRestoreBehavior(RestoreBehavior behavior);

private:
    QSharedDataPointer<Internal::SetupProjectParametersPrivate> d;
};

} // namespace qbs

#endif // Include guard
