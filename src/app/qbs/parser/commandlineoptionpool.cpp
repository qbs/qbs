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
        case CommandLineOption::LogTimeOptionType:
            option = new LogTimeOption;
            break;
        }
    }
    return option;
}

FileOption *CommandLineOptionPool::fileOption() const
{
    return static_cast<FileOption *>(getOption(CommandLineOption::FileOptionType));
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

LogTimeOption *CommandLineOptionPool::logTimeOption() const
{
    return static_cast<LogTimeOption *>(getOption(CommandLineOption::LogTimeOptionType));
}

} // namespace qbs
