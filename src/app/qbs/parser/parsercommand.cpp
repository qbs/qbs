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
#include "parsercommand.h"

#include "commandlineoption.h"
#include "commandlineoptionpool.h"

#include <logging/translator.h>
#include <tools/error.h>
#include <tools/hostosinfo.h>
#include <tools/set.h>

#include <QtCore/qmap.h>

namespace qbs {
using namespace Internal;

Command::~Command()
{
}

void Command::parse(QStringList &input)
{
    parseOptions(input);
    parseMore(input);
    if (!input.isEmpty())
        throwError(Tr::tr("Extraneous input '%1'").arg(input.join(QLatin1Char(' '))));
}

void Command::addAllToAdditionalArguments(QStringList &input)
{
    while (!input.isEmpty())
        addOneToAdditionalArguments(input.takeFirst());
}

// TODO: Stricter checking for build variants and properties.
void Command::addOneToAdditionalArguments(const QString &argument)
{
    if (argument.startsWith(QLatin1Char('-'))) {
        throwError(Tr::tr("Encountered option '%1', expected a build "
                          "configuration name or property.")
                   .arg(argument));
    }
    m_additionalArguments << argument;
}

QList<CommandLineOption::Type> Command::actualSupportedOptions() const
{
    QList<CommandLineOption::Type> options = supportedOptions();
    if (!HostOsInfo::isAnyUnixHost())
        options.removeOne(CommandLineOption::ShowProgressOptionType);
    if (type() != HelpCommandType)
        options << CommandLineOption::SettingsDirOptionType; // Valid for almost all commands.
    return options;
}

void Command::parseOptions(QStringList &input)
{
    Set<CommandLineOption *> usedOptions;
    while (!input.isEmpty()) {
        const QString optionString = input.first();
        if (!optionString.startsWith(QLatin1Char('-')))
            break;
        if (optionString == QLatin1String("--"))
            break;
        input.removeFirst();
        if (optionString.count() == 1)
            throwError(Tr::tr("Empty options are not allowed."));

        // Split up grouped short options.
        if (optionString.at(1) != QLatin1Char('-') && optionString.count() > 2) {
            QString parameter;
            for (int i = optionString.count(); --i > 0;) {
                const QChar c = optionString.at(i);
                if (c.isDigit()) {
                    parameter.prepend(c);
                } else {
                    if (!parameter.isEmpty()) {
                        input.prepend(parameter);
                        parameter.clear();
                    }
                    input.prepend(QLatin1Char('-') + c);
                }
            }
            if (!parameter.isEmpty())
                throwError(Tr::tr("Unknown numeric option '%1'.").arg(parameter));
            continue;
        }

        bool matchFound = false;
        foreach (const CommandLineOption::Type optionType, actualSupportedOptions()) {
            CommandLineOption * const option = optionPool().getOption(optionType);
            if (option->shortRepresentation() != optionString
                    && option->longRepresentation() != optionString) {
                continue;
            }
            if (usedOptions.contains(option) && !option->canAppearMoreThanOnce())
                throwError(Tr::tr("Option '%1' cannot appear more than once.").arg(optionString));
            option->parse(type(), optionString, input);
            usedOptions << option;
            matchFound = true;
            break;
        }
        if (!matchFound)
            throwError(Tr::tr("Unknown option '%1'.").arg(optionString));
    }
}

QString Command::supportedOptionsDescription() const
{
    // Sorting the options by name is nicer for the user.
    QMap<QString, const CommandLineOption *> optionMap;
    foreach (const CommandLineOption::Type opType, actualSupportedOptions()) {
        const CommandLineOption * const option = optionPool().getOption(opType);
        optionMap.insert(option->longRepresentation(), option);
    }

    QString s = Tr::tr("The possible options are:\n");
    foreach (const CommandLineOption *option, optionMap)
        s += option->description(type());
    return s;
}

void Command::throwError(const QString &reason)
{
    ErrorInfo error(Tr::tr("Invalid use of command '%1': %2").arg(representation(), reason));
    error.append(Tr::tr("Type 'qbs help %1' to see how to use this command.")
                 .arg(representation()));
    throw error;
}

void Command::parseMore(QStringList &input)
{
    addAllToAdditionalArguments(input);
}


QString ResolveCommand::shortDescription() const
{
    return Tr::tr("Resolve a project without building it.");
}

QString ResolveCommand::longDescription() const
{
    QString description = Tr::tr(
                "qbs %1 [options] [[configuration-name] [property:value] ...] ...\n")
            .arg(representation());
    description += Tr::tr("Resolves a project in one or more configuration(s).\n");
    return description += supportedOptionsDescription();
}

QString ResolveCommand::representation() const
{
    return QLatin1String("resolve");
}

static QList<CommandLineOption::Type> resolveOptions()
{
    return QList<CommandLineOption::Type>()
            << CommandLineOption::FileOptionType
            << CommandLineOption::BuildDirectoryOptionType
            << CommandLineOption::LogLevelOptionType
            << CommandLineOption::VerboseOptionType
            << CommandLineOption::QuietOptionType
            << CommandLineOption::ShowProgressOptionType
            << CommandLineOption::JobsOptionType
            << CommandLineOption::DryRunOptionType
            << CommandLineOption::ForceProbesOptionType
            << CommandLineOption::LogTimeOptionType
            << CommandLineOption::ForceOptionType;
}

QList<CommandLineOption::Type> ResolveCommand::supportedOptions() const
{
    return resolveOptions();
}

QString GenerateCommand::shortDescription() const
{
    return Tr::tr("Generate project files for another build tool.");
}

QString GenerateCommand::longDescription() const
{
    QString description = Tr::tr(
                "qbs %1 [options] [[configuration-name] [property:value] ...] ...\n")
            .arg(representation());
    description += Tr::tr("Generates files to build the project using another build tool.\n");
    return description += supportedOptionsDescription();
}

QString GenerateCommand::representation() const
{
    return QLatin1String("generate");
}

QList<CommandLineOption::Type> GenerateCommand::supportedOptions() const
{
    return QList<CommandLineOption::Type>()
            << CommandLineOption::FileOptionType
            << CommandLineOption::BuildDirectoryOptionType
            << CommandLineOption::LogLevelOptionType
            << CommandLineOption::VerboseOptionType
            << CommandLineOption::QuietOptionType
            << CommandLineOption::ShowProgressOptionType
            << CommandLineOption::InstallRootOptionType
            << CommandLineOption::LogTimeOptionType
            << CommandLineOption::GeneratorOptionType;
}

QString BuildCommand::shortDescription() const
{
    return Tr::tr("Build (parts of) a project. This is the default command.");
}

QString BuildCommand::longDescription() const
{
    QString description = Tr::tr(
                "qbs %1 [options] [[configuration-name] [property:value] ...] ...\n")
            .arg(representation());
    description += Tr::tr("Builds a project in one or more configuration(s).\n");
    return description += supportedOptionsDescription();
}

static QString buildCommandRepresentation() { return QLatin1String("build"); }

QString BuildCommand::representation() const
{
    return buildCommandRepresentation();
}

static QList<CommandLineOption::Type> buildOptions()
{
    QList<CommandLineOption::Type> options = resolveOptions();
    return options << CommandLineOption::KeepGoingOptionType
            << CommandLineOption::ProductsOptionType
            << CommandLineOption::ChangedFilesOptionType
            << CommandLineOption::ForceTimestampCheckOptionType
            << CommandLineOption::ForceOutputCheckOptionType
            << CommandLineOption::BuildNonDefaultOptionType
            << CommandLineOption::VersionOptionType
            << CommandLineOption::CommandEchoModeOptionType
            << CommandLineOption::NoInstallOptionType
            << CommandLineOption::RemoveFirstOptionType
            << CommandLineOption::WaitLockOptionType;
}

QList<CommandLineOption::Type> BuildCommand::supportedOptions() const
{
    return buildOptions();
}

QString CleanCommand::shortDescription() const
{
    return Tr::tr("Remove the files generated during a build.");
}

QString CleanCommand::longDescription() const
{
    QString description = Tr::tr(
                "qbs %1 [options] [[configuration-name] [property:value] ...] ...\n")
            .arg(representation());
    description += Tr::tr("Removes build artifacts for the given project and configuration(s).\n");
    return description += supportedOptionsDescription();
}

QString CleanCommand::representation() const
{
    return QLatin1String("clean");
}

QList<CommandLineOption::Type> CleanCommand::supportedOptions() const
{
    QList<CommandLineOption::Type> options = buildOptions();
    options.removeOne(CommandLineOption::ChangedFilesOptionType);
    options.removeOne(CommandLineOption::JobsOptionType);
    options.removeOne(CommandLineOption::BuildNonDefaultOptionType);
    options.removeOne(CommandLineOption::CommandEchoModeOptionType);
    return options;
}

QString InstallCommand::shortDescription() const
{
    return Tr::tr("Install (parts of) a project.");
}

QString InstallCommand::longDescription() const
{
    QString description = Tr::tr(
                "qbs %1 [options] [[configuration-name] [property:value] ...]\n")
            .arg(representation());
    description += Tr::tr("Install all files marked as installable "
                          "to their respective destinations.\n"
                          "The project is built first, if necessary, unless the '%1' option "
                          "is given.\n").arg(optionPool().noBuildOption()->longRepresentation());
    return description += supportedOptionsDescription();
}

QString InstallCommand::representation() const
{
    return QLatin1String("install");
}

QList<CommandLineOption::Type> installOptions()
{
    QList<CommandLineOption::Type> options = buildOptions()
            << CommandLineOption::InstallRootOptionType
            << CommandLineOption::NoBuildOptionType;
    options.removeOne(CommandLineOption::NoInstallOptionType);
    return options;
}

QList<CommandLineOption::Type> InstallCommand::supportedOptions() const
{
    return installOptions();
}

QString RunCommand::shortDescription() const
{
    return QLatin1String("Run an executable generated by building a project.");
}

QString RunCommand::longDescription() const
{
    QString description = Tr::tr(
                "qbs %1 [options] [configuration-name] [property:value] ... "
                                 "[ -- <arguments>]\n").arg(representation());
    description += Tr::tr("Run the specified product's executable with the specified arguments.\n");
    description += Tr::tr("If the project has only one product, the '%1' option may be omitted.\n")
            .arg(optionPool().productsOption()->longRepresentation());
    description += Tr::tr("The product will be built if it is not up to date; "
                          "see the '%2' command.\n").arg(buildCommandRepresentation());
    return description += supportedOptionsDescription();
}

QString RunCommand::representation() const
{
    return QLatin1String("run");
}

QList<CommandLineOption::Type> RunCommand::supportedOptions() const
{
    return installOptions();
}

void RunCommand::parseMore(QStringList &input)
{
    // Build variants and properties
    while (!input.isEmpty()) {
        const QString arg = input.takeFirst();
        if (arg == QLatin1String("--"))
            break;
        addOneToAdditionalArguments(arg);
    }

    m_targetParameters = input;
    input.clear();
}

QString ShellCommand::shortDescription() const
{
    return Tr::tr("Open a shell with a product's environment.");
}

QString ShellCommand::longDescription() const
{
    QString description = Tr::tr(
                "qbs %1 [options] [configuration-name] [property:value] ...\n")
            .arg(representation());
    description += Tr::tr("Opens a shell in the same environment that a build with the given "
                          "parameters would use.\n");
    const ProductsOption * const option = optionPool().productsOption();
    description += Tr::tr("The '%1' option may be omitted if and only if the project has "
                          "exactly one product.").arg(option->longRepresentation());
    return description += supportedOptionsDescription();
}

QString ShellCommand::representation() const
{
    return QLatin1String("shell");
}

QList<CommandLineOption::Type> ShellCommand::supportedOptions() const
{
    return QList<CommandLineOption::Type>()
            << CommandLineOption::FileOptionType
            << CommandLineOption::BuildDirectoryOptionType
            << CommandLineOption::LogLevelOptionType
            << CommandLineOption::VerboseOptionType
            << CommandLineOption::QuietOptionType
            << CommandLineOption::ShowProgressOptionType
            << CommandLineOption::ProductsOptionType;
}

QString StatusCommand::shortDescription() const
{
    return Tr::tr("Show the status of files in the project directory.");
}

QString StatusCommand::longDescription() const
{
    QString description = Tr::tr(
                "qbs %1 [options] [configuration-name] [property:value] ...\n")
            .arg(representation());
    description += Tr::tr("Lists all the files in the project directory and shows whether "
                          "they are known to qbs in the respective configuration.\n");
    return description += supportedOptionsDescription();
}

QString StatusCommand::representation() const
{
    return QLatin1String("status");
}

QList<CommandLineOption::Type> StatusCommand::supportedOptions() const
{
    return QList<CommandLineOption::Type>()
            << CommandLineOption::FileOptionType
            << CommandLineOption::BuildDirectoryOptionType
            << CommandLineOption::LogLevelOptionType
            << CommandLineOption::VerboseOptionType
            << CommandLineOption::QuietOptionType
            << CommandLineOption::ShowProgressOptionType;
}

QString UpdateTimestampsCommand::shortDescription() const
{
    return Tr::tr("Mark the build as up to date.");
}

QString UpdateTimestampsCommand::longDescription() const
{
    QString description = Tr::tr(
                "qbs %1 [options] [[configuration-name] [property:value] ...] ...\n")
            .arg(representation());
    description += Tr::tr("Update the timestamps of all build artifacts, causing the next "
            "builds of the project to do nothing if no updates to source files happen in between.\n"
            "This functionality is useful if you know that the current changes to source files "
            "are irrelevant to the build.\n"
            "NOTE: Doing this causes a discrepancy between the \"real world\" and the information "
            "in the build graph, so use with care.\n");
    return description += supportedOptionsDescription();
}

QString UpdateTimestampsCommand::representation() const
{
    return QLatin1String("update-timestamps");
}

QList<CommandLineOption::Type> UpdateTimestampsCommand::supportedOptions() const
{
    return QList<CommandLineOption::Type>()
            << CommandLineOption::FileOptionType
            << CommandLineOption::BuildDirectoryOptionType
            << CommandLineOption::LogLevelOptionType
            << CommandLineOption::VerboseOptionType
            << CommandLineOption::QuietOptionType
            << CommandLineOption::ProductsOptionType;
}

QString DumpNodesTreeCommand::shortDescription() const
{
    return Tr::tr("Dumps the nodes in the build graph to stdout.");
}

QString DumpNodesTreeCommand::longDescription() const
{
    QString description = Tr::tr(
                "qbs %1 [options] [[configuration-name] [property:value] ...] ...\n")
            .arg(representation());
    description += Tr::tr("Internal command; for debugging purposes only.\n");
    return description += supportedOptionsDescription();
}

QString DumpNodesTreeCommand::representation() const
{
    return QLatin1String("dump-nodes-tree");
}

QList<CommandLineOption::Type> DumpNodesTreeCommand::supportedOptions() const
{
    return QList<CommandLineOption::Type>()
            << CommandLineOption::LogLevelOptionType
            << CommandLineOption::VerboseOptionType
            << CommandLineOption::QuietOptionType
            << CommandLineOption::FileOptionType
            << CommandLineOption::BuildDirectoryOptionType
            << CommandLineOption::ProductsOptionType;
}

QString HelpCommand::shortDescription() const
{
    return Tr::tr("Show general or command-specific help.");
}

QString HelpCommand::longDescription() const
{
    QString description = Tr::tr("qbs %1 [<command>]\n").arg(representation());
    return description += Tr::tr("Shows either the general help or a description of "
                                 "the given command.\n");
}

QString HelpCommand::representation() const
{
    return QLatin1String("help");
}

QList<CommandLineOption::Type> HelpCommand::supportedOptions() const
{
    return QList<CommandLineOption::Type>();
}

void HelpCommand::parseMore(QStringList &input)
{
    if (input.isEmpty())
        return;
    if (input.count() > 1)
        throwError(Tr::tr("Cannot describe more than one command."));
    m_command = input.takeFirst();
    Q_ASSERT(input.isEmpty());
}

} // namespace qbs
