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
#ifndef QBS_COMMANDLINEOPTION_H
#define QBS_COMMANDLINEOPTION_H

#include "commandtype.h"

#include <tools/commandechomode.h>

#include <QtCore/qstringlist.h>

namespace qbs {

class CommandLineOption
{
public:
    enum Type {
        FileOptionType,
        BuildDirectoryOptionType,
        LogLevelOptionType, VerboseOptionType, QuietOptionType,
        JobsOptionType,
        KeepGoingOptionType,
        DryRunOptionType,
        ForceProbesOptionType,
        ShowProgressOptionType,
        ChangedFilesOptionType,
        ProductsOptionType,
        NoInstallOptionType,
        InstallRootOptionType, RemoveFirstOptionType, NoBuildOptionType,
        ForceOptionType,
        ForceTimestampCheckOptionType,
        ForceOutputCheckOptionType,
        BuildNonDefaultOptionType,
        VersionOptionType,
        LogTimeOptionType,
        CommandEchoModeOptionType,
        SettingsDirOptionType,
        GeneratorOptionType,
        WaitLockOptionType
    };

    virtual ~CommandLineOption();
    virtual QString description(CommandType command) const = 0;
    virtual QString shortRepresentation() const = 0;
    virtual QString longRepresentation() const = 0;
    virtual bool canAppearMoreThanOnce() const { return false; }

    void parse(CommandType command, const QString &representation, QStringList &input);

protected:
    CommandLineOption();
    QString getArgument(const QString &representation, QStringList &input);
    CommandType command() const { return m_command; }

private:
    virtual void doParse(const QString &representation, QStringList &input) = 0;

    CommandType m_command;
};

class FileOption : public CommandLineOption
{
public:
    QString projectFilePath() const { return m_projectFilePath; }

private:
    QString description(CommandType command) const;
    QString shortRepresentation() const;
    QString longRepresentation() const;
    void doParse(const QString &representation, QStringList &input);

private:
    QString m_projectFilePath;
};

class BuildDirectoryOption : public CommandLineOption
{
public:
    QString projectBuildDirectory() const { return m_projectBuildDirectory; }
    static QString magicProjectString();
    static QString magicProjectDirString();

private:
    QString description(CommandType command) const;
    QString shortRepresentation() const;
    QString longRepresentation() const;
    void doParse(const QString &representation, QStringList &input);

private:
    QString m_projectBuildDirectory;
};

class GeneratorOption : public CommandLineOption
{
public:
    QString generatorName() const { return m_generatorName; }

private:
    QString description(CommandType command) const;
    QString shortRepresentation() const;
    QString longRepresentation() const;
    void doParse(const QString &representation, QStringList &input);

private:
    QString m_generatorName;
};

class CountingOption : public CommandLineOption
{
public:
    int count() const { return m_count; }

protected:
    CountingOption() : m_count(0) {}

private:
    bool canAppearMoreThanOnce() const { return true; }
    void doParse(const QString & /* representation */, QStringList & /* input */) { ++m_count; }

    int m_count;
};

class VerboseOption : public CountingOption
{
    QString description(CommandType command) const;
    QString shortRepresentation() const;
    QString longRepresentation() const;
};

class QuietOption : public CountingOption
{
    QString description(CommandType command) const;
    QString shortRepresentation() const;
    QString longRepresentation() const;
};

class JobsOption : public CommandLineOption
{
public:
    JobsOption() : m_jobCount(0) {}
    int jobCount() const { return m_jobCount; }

private:
    QString description(CommandType command) const;
    QString shortRepresentation() const;
    QString longRepresentation() const;
    void doParse(const QString &representation, QStringList &input);

    int m_jobCount;
};

class OnOffOption : public CommandLineOption
{
public:
    bool enabled() const { return m_enabled; }

protected:
    OnOffOption() : m_enabled(false) {}

private:
    void doParse(const QString & /* representation */, QStringList & /* input */) { m_enabled = true; }

    bool m_enabled;
};

class KeepGoingOption : public OnOffOption
{
    QString description(CommandType command) const;
    QString shortRepresentation() const;
    QString longRepresentation() const;
};

class DryRunOption : public OnOffOption
{
    QString description(CommandType command) const;
    QString shortRepresentation() const;
    QString longRepresentation() const;
};

class ForceProbesOption : public OnOffOption
{
    QString description(CommandType command) const;
    QString shortRepresentation() const { return QString(); }
    QString longRepresentation() const;
};

class NoInstallOption : public OnOffOption
{
    QString description(CommandType command) const;
    QString shortRepresentation() const { return QString(); }
    QString longRepresentation() const;
};

class ShowProgressOption : public OnOffOption
{
public:
    QString description(CommandType command) const;
    QString shortRepresentation() const { return QString(); }
    QString longRepresentation() const;
};

class ForceOption : public OnOffOption
{
    QString description(CommandType command) const;
    QString shortRepresentation() const { return QString(); }
    QString longRepresentation() const;
};

class ForceTimeStampCheckOption : public OnOffOption
{
    QString description(CommandType command) const;
    QString shortRepresentation() const { return QString(); }
    QString longRepresentation() const;
};

class ForceOutputCheckOption : public OnOffOption
{
    QString description(CommandType command) const;
    QString shortRepresentation() const { return QString(); }
    QString longRepresentation() const;
};

class BuildNonDefaultOption : public OnOffOption
{
    QString description(CommandType command) const;
    QString shortRepresentation() const { return QString(); }
    QString longRepresentation() const;
};

class VersionOption : public OnOffOption
{
private:
    QString description(CommandType command) const;
    QString shortRepresentation() const;
    QString longRepresentation() const;

private:
    QString m_projectFilePath;
};


class StringListOption : public CommandLineOption
{
public:
    QStringList arguments() const { return m_arguments; }

private:
    void doParse(const QString &representation, QStringList &input);

    QStringList m_arguments;
};

class ChangedFilesOption : public StringListOption
{
    QString description(CommandType command) const;
    QString shortRepresentation() const { return QString(); }
    QString longRepresentation() const;
};

class ProductsOption : public StringListOption
{
public:
    QString description(CommandType command) const;
    QString shortRepresentation() const;
    QString longRepresentation() const;
};

class LogLevelOption : public CommandLineOption
{
public:
    LogLevelOption();
    int logLevel() const { return m_logLevel; }

private:
    QString description(CommandType command) const;
    QString shortRepresentation() const { return QString(); }
    QString longRepresentation() const;
    void doParse(const QString &representation, QStringList &input);

    int m_logLevel;
};

class InstallRootOption : public CommandLineOption
{
public:
    InstallRootOption();

    QString installRoot() const { return m_installRoot; }
    bool useSysroot() const { return m_useSysroot; }

    QString description(CommandType command) const;
    QString shortRepresentation() const { return QString(); }
    QString longRepresentation() const;

private:
    void doParse(const QString &representation, QStringList &input);

    QString m_installRoot;
    bool m_useSysroot;
};

class RemoveFirstOption : public OnOffOption
{
public:
    QString description(CommandType command) const;
    QString shortRepresentation() const { return QString(); }
    QString longRepresentation() const;
};

class NoBuildOption : public OnOffOption
{
public:
    QString description(CommandType command) const;
    QString shortRepresentation() const { return QString(); }
    QString longRepresentation() const;
};

class LogTimeOption : public OnOffOption
{
public:
    QString description(CommandType command) const;
    QString shortRepresentation() const;
    QString longRepresentation() const;
};

class CommandEchoModeOption : public CommandLineOption
{
public:
    CommandEchoModeOption();

    QString description(CommandType command) const;
    QString shortRepresentation() const { return QString(); }
    QString longRepresentation() const;
    CommandEchoMode commandEchoMode() const;

private:
    void doParse(const QString &representation, QStringList &input);

    CommandEchoMode m_echoMode = CommandEchoModeInvalid;
};

class SettingsDirOption : public CommandLineOption
{
public:
    SettingsDirOption();

    QString settingsDir() const { return m_settingsDir; }

    QString description(CommandType command) const;
    QString shortRepresentation() const { return QString(); }
    QString longRepresentation() const;

private:
    void doParse(const QString &representation, QStringList &input);

    QString m_settingsDir;
};

class WaitLockOption : public OnOffOption
{
public:
    QString description(CommandType command) const;
    QString shortRepresentation() const { return QString(); }
    QString longRepresentation() const;
};

} // namespace qbs

#endif // QBS_COMMANDLINEOPTION_H
