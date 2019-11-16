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
#ifndef QBS_PARSER_COMMAND_H
#define QBS_PARSER_COMMAND_H

#include "commandlineoption.h"
#include "commandtype.h"

#include <set>

namespace qbs {
class CommandLineOptionPool;

class Command
{
public:
    virtual ~Command();

    virtual CommandType type() const = 0;
    virtual QString shortDescription() const = 0;
    virtual QString longDescription() const = 0;
    virtual QString representation() const = 0;

    void parse(QStringList &input);
    QStringList additionalArguments() const { return m_additionalArguments; }
    bool canResolve() const;

protected:
    Command(CommandLineOptionPool &optionPool) : m_optionPool(optionPool) {}

    const CommandLineOptionPool &optionPool() const { return m_optionPool; }
    QString supportedOptionsDescription() const;
    [[noreturn]] void throwError(const QString &reason);

    virtual void parseNext(QStringList &input);

private:
    QList<CommandLineOption::Type> actualSupportedOptions() const;
    void parseOption(QStringList &input);
    void parsePropertyAssignment(const QString &argument);

    virtual QList<CommandLineOption::Type> supportedOptions() const = 0;

    QStringList m_additionalArguments;
    std::set<CommandLineOption *> m_usedOptions;
    const CommandLineOptionPool &m_optionPool;
};

class ResolveCommand : public Command
{
public:
    ResolveCommand(CommandLineOptionPool &optionPool) : Command(optionPool) {}

private:
    CommandType type() const override { return ResolveCommandType; }
    QString shortDescription() const override;
    QString longDescription() const override;
    QString representation() const override;
    QList<CommandLineOption::Type> supportedOptions() const override;
};

class GenerateCommand : public Command
{
public:
    GenerateCommand(CommandLineOptionPool &optionPool) : Command(optionPool) {}

private:
    CommandType type() const override { return GenerateCommandType; }
    QString shortDescription() const override;
    QString longDescription() const override;
    QString representation() const override;
    QList<CommandLineOption::Type> supportedOptions() const override;
};

class BuildCommand : public Command
{
public:
    BuildCommand(CommandLineOptionPool &optionPool) : Command(optionPool) {}

private:
    CommandType type() const override { return BuildCommandType; }
    QString shortDescription() const override;
    QString longDescription() const override;
    QString representation() const override;
    QList<CommandLineOption::Type> supportedOptions() const override;
};

class CleanCommand : public Command
{
public:
    CleanCommand(CommandLineOptionPool &optionPool) : Command(optionPool) {}

private:
    CommandType type() const override { return CleanCommandType; }
    QString shortDescription() const override;
    QString longDescription() const override;
    QString representation() const override;
    QList<CommandLineOption::Type> supportedOptions() const override;
};

class InstallCommand : public Command
{
public:
    InstallCommand(CommandLineOptionPool &optionPool) : Command(optionPool) {}

private:
    CommandType type() const override { return InstallCommandType; }
    QString shortDescription() const override;
    QString longDescription() const override;
    QString representation() const override;
    QList<CommandLineOption::Type> supportedOptions() const override;
};

class RunCommand : public Command
{
public:
    RunCommand(CommandLineOptionPool &optionPool) : Command(optionPool) {}
    QStringList targetParameters() const { return m_targetParameters; }

private:
    CommandType type() const override { return RunCommandType; }
    QString shortDescription() const override;
    QString longDescription() const override;
    QString representation() const override;
    QList<CommandLineOption::Type> supportedOptions() const override;
    void parseNext(QStringList &input) override;

    QStringList m_targetParameters;
};

class ShellCommand : public Command
{
public:
    ShellCommand(CommandLineOptionPool &optionPool) : Command(optionPool) {}

private:
    CommandType type() const override { return ShellCommandType; }
    QString shortDescription() const override;
    QString longDescription() const override;
    QString representation() const override;
    QList<CommandLineOption::Type> supportedOptions() const override;
};

// TODO: It seems wrong that a configuration has to be given here. Ideally, this command would just track *all* files regardless of conditions. Is that possible?
class StatusCommand : public Command
{
public:
    StatusCommand(CommandLineOptionPool &optionPool) : Command(optionPool) {}

private:
    CommandType type() const override { return StatusCommandType; }
    QString shortDescription() const override;
    QString longDescription() const override;
    QString representation() const override;
    QList<CommandLineOption::Type> supportedOptions() const override;
};

class UpdateTimestampsCommand : public Command
{
public:
    UpdateTimestampsCommand(CommandLineOptionPool &optionPool) : Command(optionPool) {}

private:
    CommandType type() const override { return UpdateTimestampsCommandType; }
    QString shortDescription() const override;
    QString longDescription() const override;
    QString representation() const override;
    QList<CommandLineOption::Type> supportedOptions() const override;
};

class DumpNodesTreeCommand : public Command
{
public:
    DumpNodesTreeCommand(CommandLineOptionPool &optionPool) : Command(optionPool) {}

private:
    CommandType type() const override{ return DumpNodesTreeCommandType; }
    QString shortDescription() const override;
    QString longDescription() const override;
    QString representation() const override;
    QList<CommandLineOption::Type> supportedOptions() const override;
};

class ListProductsCommand : public Command
{
public:
    ListProductsCommand(CommandLineOptionPool &optionPool) : Command(optionPool) {}

private:
    CommandType type() const override { return ListProductsCommandType; }
    QString shortDescription() const override;
    QString longDescription() const override;
    QString representation() const override;
    QList<CommandLineOption::Type> supportedOptions() const override;
};

class HelpCommand : public Command
{
public:
    HelpCommand(CommandLineOptionPool &optionPool) : Command(optionPool) {}
    QString commandToDescribe() const { return m_command; }

private:
    CommandType type() const override { return HelpCommandType; }
    QString shortDescription() const override;
    QString longDescription() const override;
    QString representation() const override;
    QList<CommandLineOption::Type> supportedOptions() const override;
    void parseNext(QStringList &input) override;

    QString m_command;
};

class VersionCommand : public Command
{
public:
    VersionCommand(CommandLineOptionPool &optionPool) : Command(optionPool) {}

private:
    CommandType type() const override { return VersionCommandType; }
    QString shortDescription() const override;
    QString longDescription() const override;
    QString representation() const override;
    QList<CommandLineOption::Type> supportedOptions() const override;
    void parseNext(QStringList &input) override;
};

class SessionCommand : public Command
{
public:
    SessionCommand(CommandLineOptionPool &optionPool) : Command(optionPool) {}

private:
    CommandType type() const override { return SessionCommandType; }
    QString shortDescription() const override;
    QString longDescription() const override;
    QString representation() const override;
    QList<CommandLineOption::Type> supportedOptions() const override { return {}; }
    void parseNext(QStringList &input) override;
};

} // namespace qbs

#endif // QBS_PARSER_COMMAND_H
