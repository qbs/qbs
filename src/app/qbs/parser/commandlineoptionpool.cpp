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
        case CommandLineOption::ShowProgressOptionType:
            option = new ShowProgressOption;
            break;
        case CommandLineOption::ChangedFilesOptionType:
            option = new ChangedFilesOption;
            break;
        case CommandLineOption::ProductsOptionType:
            option = new ProductsOption;
            break;
        case CommandLineOption::AllArtifactsOptionType:
            option = new AllArtifactsOption;
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
        case CommandLineOption::ForceOptionType:
            option = new ForceOption;
            break;
        case CommandLineOption::ForceTimestampCheckOptionType:
            option = new ForceTimeStampCheckOption;
            break;
        case CommandLineOption::BuildNonDefaultOptionType:
            option = new BuildNonDefaultOption;
            break;
        case CommandLineOption::VersionOptionType:
            option = new VersionOption;
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
        case CommandLineOption::GeneratorOptionType:
            option = new GeneratorOption;
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

AllArtifactsOption *CommandLineOptionPool::allArtifactsOption() const
{
    return static_cast<AllArtifactsOption *>(getOption(CommandLineOption::AllArtifactsOptionType));
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

ForceOption *CommandLineOptionPool::forceOption() const
{
    return static_cast<ForceOption *>(getOption(CommandLineOption::ForceOptionType));
}

ForceTimeStampCheckOption *CommandLineOptionPool::forceTimestampCheckOption() const
{
    return static_cast<ForceTimeStampCheckOption *>(
                getOption(CommandLineOption::ForceTimestampCheckOptionType));
}

BuildNonDefaultOption *CommandLineOptionPool::buildNonDefaultOption() const
{
    return static_cast<BuildNonDefaultOption *>(
                getOption(CommandLineOption::BuildNonDefaultOptionType));
}

VersionOption *CommandLineOptionPool::versionOption() const
{
    return static_cast<VersionOption *>(
                getOption(CommandLineOption::VersionOptionType));
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

GeneratorOption *CommandLineOptionPool::generatorOption() const
{
    return static_cast<GeneratorOption *>(getOption(CommandLineOption::GeneratorOptionType));
}

} // namespace qbs
