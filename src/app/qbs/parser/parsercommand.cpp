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
#include <tools/qbsassert.h>
#include <tools/stlutils.h>
#include <tools/qttools.h>

#include <QtCore/qmap.h>

namespace qbs {
using namespace Internal;

Command::~Command() = default;

void Command::parse(QStringList &input)
{
    while (!input.empty())
        parseNext(input);
}

bool Command::canResolve() const
{
    return supportedOptions().contains(CommandLineOption::FileOptionType);
}

void Command::parsePropertyAssignment(const QString &argument)
{
    const auto throwError = [argument](const QString &msgTemplate) {
        ErrorInfo error(msgTemplate.arg(argument));
        error.append(QLatin1String("Expected an assignment of the form <property>:<value>, "
                                   "profile:<profile-name> or config:<configuration-name>."));
        throw error;
    };
    if (argument.startsWith(QLatin1Char('-')))
        throwError(Tr::tr("Unexpected option '%1'."));
    const int sepPos = argument.indexOf(QLatin1Char(':'));
    if (sepPos == -1)
        throwError(Tr::tr("Unexpected command line parameter '%1'."));
    if (sepPos == 0)
        throwError(Tr::tr("Empty key not allowed in assignment '%1'."));
    if (!canResolve() && argument.contains(QLatin1Char(':'))
            && !argument.startsWith(QLatin1String("config:"))) {
        throw ErrorInfo(Tr::tr("The '%1' command does not support property assignments.")
                        .arg(representation()));
    }
    m_additionalArguments << argument;
}

QList<CommandLineOption::Type> Command::actualSupportedOptions() const
{
    QList<CommandLineOption::Type> options = supportedOptions();
    if (type() != HelpCommandType)
        options.push_back(CommandLineOption::SettingsDirOptionType); // Valid for almost all commands.
    return options;
}

void Command::parseOption(QStringList &input)
{
    const QString optionString = input.front();
    QBS_CHECK(optionString.startsWith(QLatin1Char('-')));
    input.removeFirst();
    if (optionString.size() == 1)
        throwError(Tr::tr("Empty options are not allowed."));

    // Split up grouped short options.
    if (optionString.at(1) != QLatin1Char('-') && optionString.size() > 2) {
        QString parameter;
        for (int i = optionString.size(); --i > 0;) {
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
        return;
    }

    bool matchFound = false;
    const auto optionTypes = actualSupportedOptions();
    for (const CommandLineOption::Type optionType : optionTypes) {
        CommandLineOption * const option = optionPool().getOption(optionType);
        if (option->shortRepresentation() != optionString
                && option->longRepresentation() != optionString) {
            continue;
        }
        if (contains(m_usedOptions, option) && !option->canAppearMoreThanOnce())
            throwError(Tr::tr("Option '%1' cannot appear more than once.").arg(optionString));
        option->parse(type(), optionString, input);
        m_usedOptions.insert(option);
        matchFound = true;
        break;
    }
    if (!matchFound)
        throwError(Tr::tr("Unknown option '%1'.").arg(optionString));
}

void Command::parseNext(QStringList &input)
{
    QBS_CHECK(!input.empty());
    if (input.front().startsWith(QLatin1Char('-')))
        parseOption(input);
    else
        parsePropertyAssignment(input.takeFirst());
}

QString Command::supportedOptionsDescription() const
{
    // Sorting the options by name is nicer for the user.
    QMap<QString, const CommandLineOption *> optionMap;
    const auto opTypes = actualSupportedOptions();
    for (const CommandLineOption::Type opType : opTypes) {
        const CommandLineOption * const option = optionPool().getOption(opType);
        optionMap.insert(option->longRepresentation(), option);
    }

    QString s = Tr::tr("The possible options are:\n");
    for (const CommandLineOption *option : qAsConst(optionMap))
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


QString ResolveCommand::shortDescription() const
{
    return Tr::tr("Resolve a project without building it.");
}

QString ResolveCommand::longDescription() const
{
    QString description = Tr::tr(
                "qbs %1 [options] [[config:<configuration-name>] [<property>:<value>] ...] ...\n")
            .arg(representation());
    description += Tr::tr("Resolves a project in one or more configuration(s).\n");
    return description += supportedOptionsDescription();
}

QString ResolveCommand::representation() const
{
    return QStringLiteral("resolve");
}

static QList<CommandLineOption::Type> resolveOptions()
{
    return {CommandLineOption::FileOptionType,
            CommandLineOption::BuildDirectoryOptionType,
            CommandLineOption::LogLevelOptionType,
            CommandLineOption::VerboseOptionType,
            CommandLineOption::QuietOptionType,
            CommandLineOption::ShowProgressOptionType,
            CommandLineOption::DryRunOptionType,
            CommandLineOption::ForceProbesOptionType,
            CommandLineOption::LogTimeOptionType,
            CommandLineOption::DisableFallbackProviderType};
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
                "qbs %1 [options] [[config:<configuration-name>] [<property>:<value>] ...] ...\n")
            .arg(representation());
    description += Tr::tr("Generates files to build the project using another build tool.\n");
    return description += supportedOptionsDescription();
}

QString GenerateCommand::representation() const
{
    return QStringLiteral("generate");
}

QList<CommandLineOption::Type> GenerateCommand::supportedOptions() const
{
    return {CommandLineOption::FileOptionType,
            CommandLineOption::BuildDirectoryOptionType,
            CommandLineOption::LogLevelOptionType,
            CommandLineOption::VerboseOptionType,
            CommandLineOption::QuietOptionType,
            CommandLineOption::ShowProgressOptionType,
            CommandLineOption::InstallRootOptionType,
            CommandLineOption::LogTimeOptionType,
            CommandLineOption::GeneratorOptionType};
}

QString BuildCommand::shortDescription() const
{
    return Tr::tr("Build (parts of) a project. This is the default command.");
}

QString BuildCommand::longDescription() const
{
    QString description = Tr::tr(
                "qbs %1 [options] [[config:<configuration-name>] [<property>:<value>] ...] ...\n")
            .arg(representation());
    description += Tr::tr("Builds a project in one or more configuration(s).\n");
    return description += supportedOptionsDescription();
}

static QString buildCommandRepresentation() { return QStringLiteral("build"); }

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
            << CommandLineOption::JobsOptionType
            << CommandLineOption::CommandEchoModeOptionType
            << CommandLineOption::NoInstallOptionType
            << CommandLineOption::RemoveFirstOptionType
            << CommandLineOption::JobLimitsOptionType
            << CommandLineOption::RespectProjectJobLimitsOptionType
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
                "qbs %1 [options] [config:<configuration-name>] ...\n")
            .arg(representation());
    description += Tr::tr("Removes build artifacts for the given configuration(s).\n");
    return description += supportedOptionsDescription();
}

QString CleanCommand::representation() const
{
    return QStringLiteral("clean");
}

QList<CommandLineOption::Type> CleanCommand::supportedOptions() const
{
    return {CommandLineOption::BuildDirectoryOptionType,
            CommandLineOption::DryRunOptionType,
            CommandLineOption::KeepGoingOptionType,
            CommandLineOption::LogTimeOptionType,
            CommandLineOption::ProductsOptionType,
            CommandLineOption::QuietOptionType,
            CommandLineOption::SettingsDirOptionType,
            CommandLineOption::ShowProgressOptionType,
            CommandLineOption::VerboseOptionType};
}

QString InstallCommand::shortDescription() const
{
    return Tr::tr("Install (parts of) a project.");
}

QString InstallCommand::longDescription() const
{
    QString description = Tr::tr(
                "qbs %1 [options] [[config:<configuration-name>] [<property>:<value>] ...]\n")
            .arg(representation());
    description += Tr::tr("Install all files marked as installable "
                          "to their respective destinations.\n"
                          "The project is built first, if necessary, unless the '%1' option "
                          "is given.\n").arg(optionPool().noBuildOption()->longRepresentation());
    return description += supportedOptionsDescription();
}

QString InstallCommand::representation() const
{
    return QStringLiteral("install");
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
    return QStringLiteral("Run an executable generated by building a project.");
}

QString RunCommand::longDescription() const
{
    QString description = Tr::tr(
                "qbs %1 [options] [config:<configuration-name>] [<property>:<value>] ... "
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
    return QStringLiteral("run");
}

QList<CommandLineOption::Type> RunCommand::supportedOptions() const
{
    return QList<CommandLineOption::Type>() << installOptions()
                                            << CommandLineOption::RunEnvConfigOptionType;
}

void RunCommand::parseNext(QStringList &input)
{
    QBS_CHECK(!input.empty());
    if (input.front() != QLatin1String("--")) {
        Command::parseNext(input);
        return;
    }
    input.removeFirst();
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
                "qbs %1 [options] [config:<configuration-name>] [<property>:<value>] ...\n")
            .arg(representation());
    description += Tr::tr("Opens a shell in the same environment that a build with the given "
                          "parameters would use.\n");
    return description += supportedOptionsDescription();
}

QString ShellCommand::representation() const
{
    return QStringLiteral("shell");
}

QList<CommandLineOption::Type> ShellCommand::supportedOptions() const
{
    return {CommandLineOption::FileOptionType,
            CommandLineOption::BuildDirectoryOptionType,
            CommandLineOption::ProductsOptionType};
}

QString StatusCommand::shortDescription() const
{
    return Tr::tr("Show the status of files in the project directory.");
}

QString StatusCommand::longDescription() const
{
    QString description = Tr::tr("qbs %1 [options] [config:<configuration-name>]\n")
            .arg(representation());
    description += Tr::tr("Lists all the files in the project directory and shows whether "
                          "they are known to qbs in the respective configuration.\n");
    return description += supportedOptionsDescription();
}

QString StatusCommand::representation() const
{
    return QStringLiteral("status");
}

QList<CommandLineOption::Type> StatusCommand::supportedOptions() const
{
    return {CommandLineOption::BuildDirectoryOptionType};
}

QString UpdateTimestampsCommand::shortDescription() const
{
    return Tr::tr("Mark the build as up to date.");
}

QString UpdateTimestampsCommand::longDescription() const
{
    QString description = Tr::tr(
                "qbs %1 [options] [config:<configuration-name>] ...\n")
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
    return QStringLiteral("update-timestamps");
}

QList<CommandLineOption::Type> UpdateTimestampsCommand::supportedOptions() const
{
    return {CommandLineOption::BuildDirectoryOptionType,
            CommandLineOption::LogLevelOptionType,
            CommandLineOption::VerboseOptionType,
            CommandLineOption::QuietOptionType,
            CommandLineOption::ProductsOptionType};
}

QString DumpNodesTreeCommand::shortDescription() const
{
    return Tr::tr("Dumps the nodes in the build graph to stdout.");
}

QString DumpNodesTreeCommand::longDescription() const
{
    QString description = Tr::tr("qbs %1 [options] [config:<configuration-name>] ...\n")
            .arg(representation());
    description += Tr::tr("Internal command; for debugging purposes only.\n");
    return description += supportedOptionsDescription();
}

QString DumpNodesTreeCommand::representation() const
{
    return QStringLiteral("dump-nodes-tree");
}

QList<CommandLineOption::Type> DumpNodesTreeCommand::supportedOptions() const
{
    return {CommandLineOption::BuildDirectoryOptionType,
            CommandLineOption::ProductsOptionType};
}

QString ListProductsCommand::shortDescription() const
{
    return Tr::tr("Lists all products in the project, including sub-projects.");
}

QString ListProductsCommand::longDescription() const
{
    QString description = Tr::tr("qbs %1 [options] [[config:<configuration-name>] "
                                 "[<property>:<value>] ...] ...\n").arg(representation());
    return description += supportedOptionsDescription();
}

QString ListProductsCommand::representation() const
{
    return QStringLiteral("list-products");
}

QList<CommandLineOption::Type> ListProductsCommand::supportedOptions() const
{
    return {CommandLineOption::FileOptionType,
            CommandLineOption::BuildDirectoryOptionType};
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
    return QStringLiteral("help");
}

QList<CommandLineOption::Type> HelpCommand::supportedOptions() const
{
    return {};
}

void HelpCommand::parseNext(QStringList &input)
{
    if (input.empty())
        return;
    if (input.size() > 1)
        throwError(Tr::tr("Cannot describe more than one command."));
    m_command = input.takeFirst();
    QBS_CHECK(input.empty());
}

QString VersionCommand::shortDescription() const
{
    return Tr::tr("Print the Qbs version number to stdout.");
}

QString VersionCommand::longDescription() const
{
    QString description = Tr::tr("qbs %1\n").arg(representation());
    return description += Tr::tr("%1\n").arg(shortDescription());
}

QString VersionCommand::representation() const
{
    return QStringLiteral("show-version");
}

QList<CommandLineOption::Type> VersionCommand::supportedOptions() const
{
    return {};
}

void VersionCommand::parseNext(QStringList &input)
{
    QBS_CHECK(!input.empty());
    throwError(Tr::tr("This command takes no arguments."));
}

QString SessionCommand::shortDescription() const
{
    return Tr::tr("Starts a session for an IDE.");
}

QString SessionCommand::longDescription() const
{
    QString description = Tr::tr("qbs %1\n").arg(representation());
    return description += Tr::tr("Communicates on stdin and stdout via a JSON-based API.\n"
                                 "Intended for use with other tools, such as IDEs.\n");
}

QString SessionCommand::representation() const
{
    return QLatin1String("session");
}

void SessionCommand::parseNext(QStringList &input)
{
    QBS_CHECK(!input.empty());
    throwError(Tr::tr("This command takes no arguments."));
}

} // namespace qbs
