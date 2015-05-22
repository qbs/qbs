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
#ifndef QBS_COMMANDLINEOPTION_H
#define QBS_COMMANDLINEOPTION_H

#include "commandtype.h"

#include <tools/commandechomode.h>

#include <QStringList>

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
        ShowProgressOptionType,
        ChangedFilesOptionType,
        ProductsOptionType,
        AllArtifactsOptionType,
        NoInstallOptionType,
        InstallRootOptionType, RemoveFirstOptionType, NoBuildOptionType,
        ForceOptionType,
        ForceTimestampCheckOptionType,
        BuildNonDefaultOptionType,
        VersionOptionType,
        LogTimeOptionType,
        CommandEchoModeOptionType,
        SettingsDirOptionType,
        GeneratorOptionType
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

class AllArtifactsOption : public OnOffOption
{
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

    CommandEchoMode m_echoMode;
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

} // namespace qbs

#endif // QBS_COMMANDLINEOPTION_H
