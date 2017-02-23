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
#include "commandlineoptionpool.h"

namespace qbs {

CommandLineOptionPool::~CommandLineOptionPool()
{
    qDeleteAll(m_options);
}

CommandLineOption *CommandLineOptionPool::getOption(CommandLineOption::Type type) const
{
    CommandLineOption *& option = m_options[type];
    if (!option) {
        switch (type) {
        case CommandLineOption::FileOptionType:
            option = new FileOption;
            break;
        case CommandLineOption::BuildDirectoryOptionType:
            option = new BuildDirectoryOption;
            break;
        case CommandLineOption::LogLevelOptionType:
            option = new LogLevelOption;
            break;
        case CommandLineOption::VerboseOptionType:
            option = new VerboseOption;
            break;
        case CommandLineOption::QuietOptionType:
            option = new QuietOption;
            break;
        case CommandLineOption::JobsOptionType:
            option = new JobsOption;
            break;
        case CommandLineOption::KeepGoingOptionType:
            option = new KeepGoingOption;
            break;
        case CommandLineOption::DryRunOptionType:
            option = new DryRunOption;
            break;
        case CommandLineOption::ForceProbesOptionType:
            option = new ForceProbesOption;
            break;
        case CommandLineOption::ShowProgressOptionType:
            option = new ShowProgressOption;
            break;
        case CommandLineOption::ChangedFilesOptionType:
            option = new ChangedFilesOption;
            break;
        case CommandLineOption::ProductsOptionType:
            option = new ProductsOption;
            break;
        case CommandLineOption::NoInstallOptionType:
            option = new NoInstallOption;
            break;
        case CommandLineOption::InstallRootOptionType:
            option = new InstallRootOption;
            break;
        case CommandLineOption::RemoveFirstOptionType:
            option = new RemoveFirstOption;
            break;
        case CommandLineOption::NoBuildOptionType:
            option = new NoBuildOption;
            break;
        case CommandLineOption::ForceTimestampCheckOptionType:
            option = new ForceTimeStampCheckOption;
            break;
        case CommandLineOption::ForceOutputCheckOptionType:
            option = new ForceOutputCheckOption;
            break;
        case CommandLineOption::BuildNonDefaultOptionType:
            option = new BuildNonDefaultOption;
            break;
        case CommandLineOption::LogTimeOptionType:
            option = new LogTimeOption;
            break;
        case CommandLineOption::CommandEchoModeOptionType:
            option = new CommandEchoModeOption;
            break;
        case CommandLineOption::SettingsDirOptionType:
            option = new SettingsDirOption;
            break;
        case CommandLineOption::JobLimitsOptionType:
            option = new JobLimitsOption;
            break;
        case CommandLineOption::RespectProjectJobLimitsOptionType:
            option = new RespectProjectJobLimitsOption;
            break;
        case CommandLineOption::GeneratorOptionType:
            option = new GeneratorOption;
            break;
        case CommandLineOption::WaitLockOptionType:
            option = new WaitLockOption;
            break;
        case CommandLineOption::DisableFallbackProviderType:
            option = new DisableFallbackProviderOption;
            break;
        case CommandLineOption::RunEnvConfigOptionType:
            option = new RunEnvConfigOption;
            break;
        default:
            qFatal("Unknown option type %d", type);
        }
    }
    return option;
}

FileOption *CommandLineOptionPool::fileOption() const
{
    return static_cast<FileOption *>(getOption(CommandLineOption::FileOptionType));
}

BuildDirectoryOption *CommandLineOptionPool::buildDirectoryOption() const
{
    return static_cast<BuildDirectoryOption *>(getOption(CommandLineOption::BuildDirectoryOptionType));
}

LogLevelOption *CommandLineOptionPool::logLevelOption() const
{
    return static_cast<LogLevelOption *>(getOption(CommandLineOption::LogLevelOptionType));
}

VerboseOption *CommandLineOptionPool::verboseOption() const
{
    return static_cast<VerboseOption *>(getOption(CommandLineOption::VerboseOptionType));
}

QuietOption *CommandLineOptionPool::quietOption() const
{
    return static_cast<QuietOption *>(getOption(CommandLineOption::QuietOptionType));
}

ShowProgressOption *CommandLineOptionPool::showProgressOption() const
{
    return static_cast<ShowProgressOption *>(getOption(CommandLineOption::ShowProgressOptionType));
}

DryRunOption *CommandLineOptionPool::dryRunOption() const
{
    return static_cast<DryRunOption *>(getOption(CommandLineOption::DryRunOptionType));
}

ForceProbesOption *CommandLineOptionPool::forceProbesOption() const
{
    return static_cast<ForceProbesOption *>(getOption(CommandLineOption::ForceProbesOptionType));
}

ChangedFilesOption *CommandLineOptionPool::changedFilesOption() const
{
    return static_cast<ChangedFilesOption *>(getOption(CommandLineOption::ChangedFilesOptionType));
}

KeepGoingOption *CommandLineOptionPool::keepGoingOption() const
{
    return static_cast<KeepGoingOption *>(getOption(CommandLineOption::KeepGoingOptionType));
}

JobsOption *CommandLineOptionPool::jobsOption() const
{
    return static_cast<JobsOption *>(getOption(CommandLineOption::JobsOptionType));
}

ProductsOption *CommandLineOptionPool::productsOption() const
{
    return static_cast<ProductsOption *>(getOption(CommandLineOption::ProductsOptionType));
}

NoInstallOption *CommandLineOptionPool::noInstallOption() const
{
    return static_cast<NoInstallOption *>(getOption(CommandLineOption::NoInstallOptionType));
}

InstallRootOption *CommandLineOptionPool::installRootOption() const
{
    return static_cast<InstallRootOption *>(getOption(CommandLineOption::InstallRootOptionType));
}

RemoveFirstOption *CommandLineOptionPool::removeFirstoption() const
{
    return static_cast<RemoveFirstOption *>(getOption(CommandLineOption::RemoveFirstOptionType));
}

NoBuildOption *CommandLineOptionPool::noBuildOption() const
{
    return static_cast<NoBuildOption *>(getOption(CommandLineOption::NoBuildOptionType));
}

ForceTimeStampCheckOption *CommandLineOptionPool::forceTimestampCheckOption() const
{
    return static_cast<ForceTimeStampCheckOption *>(
                getOption(CommandLineOption::ForceTimestampCheckOptionType));
}

ForceOutputCheckOption *CommandLineOptionPool::forceOutputCheckOption() const
{
    return static_cast<ForceOutputCheckOption *>(
                getOption(CommandLineOption::ForceOutputCheckOptionType));
}

BuildNonDefaultOption *CommandLineOptionPool::buildNonDefaultOption() const
{
    return static_cast<BuildNonDefaultOption *>(
                getOption(CommandLineOption::BuildNonDefaultOptionType));
}

LogTimeOption *CommandLineOptionPool::logTimeOption() const
{
    return static_cast<LogTimeOption *>(getOption(CommandLineOption::LogTimeOptionType));
}

CommandEchoModeOption *CommandLineOptionPool::commandEchoModeOption() const
{
    return static_cast<CommandEchoModeOption *>(
                getOption(CommandLineOption::CommandEchoModeOptionType));
}

SettingsDirOption *CommandLineOptionPool::settingsDirOption() const
{
    return static_cast<SettingsDirOption *>(getOption(CommandLineOption::SettingsDirOptionType));
}

JobLimitsOption *CommandLineOptionPool::jobLimitsOption() const
{
    return static_cast<JobLimitsOption *>(getOption(CommandLineOption::JobLimitsOptionType));
}

RespectProjectJobLimitsOption *CommandLineOptionPool::respectProjectJobLimitsOption() const
{
    return static_cast<RespectProjectJobLimitsOption *>(
                getOption(CommandLineOption::RespectProjectJobLimitsOptionType));
}

GeneratorOption *CommandLineOptionPool::generatorOption() const
{
    return static_cast<GeneratorOption *>(getOption(CommandLineOption::GeneratorOptionType));
}

WaitLockOption *CommandLineOptionPool::waitLockOption() const
{
    return static_cast<WaitLockOption *>(getOption(CommandLineOption::WaitLockOptionType));
}

DisableFallbackProviderOption *CommandLineOptionPool::disableFallbackProviderOption() const
{
    return static_cast<DisableFallbackProviderOption *>(
                getOption(CommandLineOption::DisableFallbackProviderType));
}

RunEnvConfigOption *CommandLineOptionPool::runEnvConfigOption() const
{
    return static_cast<RunEnvConfigOption *>(getOption(CommandLineOption::RunEnvConfigOptionType));
}

} // namespace qbs
