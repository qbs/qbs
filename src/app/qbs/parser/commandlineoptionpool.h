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
#ifndef QBS_COMMANDLINEOPTIONPOOL_H
#define QBS_COMMANDLINEOPTIONPOOL_H

#include "commandlineoption.h"

#include <QtCore/qhash.h>

namespace qbs {

class CommandLineOptionPool
{
public:
    ~CommandLineOptionPool();

    CommandLineOption *getOption(CommandLineOption::Type type) const;
    FileOption *fileOption() const;
    BuildDirectoryOption *buildDirectoryOption() const;
    LogLevelOption *logLevelOption() const;
    VerboseOption *verboseOption() const;
    QuietOption *quietOption() const;
    ShowProgressOption *showProgressOption() const;
    DryRunOption *dryRunOption() const;
    ForceProbesOption *forceProbesOption() const;
    ChangedFilesOption *changedFilesOption() const;
    KeepGoingOption *keepGoingOption() const;
    JobsOption *jobsOption() const;
    ProductsOption *productsOption() const;
    NoInstallOption *noInstallOption() const;
    InstallRootOption *installRootOption() const;
    RemoveFirstOption *removeFirstoption() const;
    NoBuildOption *noBuildOption() const;
    ForceTimeStampCheckOption *forceTimestampCheckOption() const;
    ForceOutputCheckOption *forceOutputCheckOption() const;
    BuildNonDefaultOption *buildNonDefaultOption() const;
    LogTimeOption *logTimeOption() const;
    CommandEchoModeOption *commandEchoModeOption() const;
    SettingsDirOption *settingsDirOption() const;
    JobLimitsOption *jobLimitsOption() const;
    RespectProjectJobLimitsOption *respectProjectJobLimitsOption() const;
    GeneratorOption *generatorOption() const;
    WaitLockOption *waitLockOption() const;
    DisableFallbackProviderOption *disableFallbackProviderOption() const;
    RunEnvConfigOption *runEnvConfigOption() const;

private:
    mutable QHash<CommandLineOption::Type, CommandLineOption *> m_options;
};

} // namespace qbs

#endif // QBS_COMMANDLINEOPTIONPOOL_H
