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
#ifndef QBS_COMMANDLINEOPTION_H
#define QBS_COMMANDLINEOPTION_H

#include "commandtype.h"

#include <QStringList>

namespace qbs {

class CommandLineOption
{
public:
    enum Type {
        FileOptionType,
        LogLevelOptionType, VerboseOptionType, QuietOptionType,
        JobsOptionType,
        KeepGoingOptionType,
        DryRunOptionType,
        ShowProgressOptionType,
        ChangedFilesOptionType,
        ProductsOptionType,
        AllArtifactsOptionType,
        InstallRootOptionType, RemoveFirstOptionType, NoBuildOptionType,
        ForceOptionType,
        ForceTimestampCheckOptionType,
        LogTimeOptionType
    };

    virtual ~CommandLineOption();
    virtual QString description(CommandType command) const = 0;
    virtual QString shortRepresentation() const = 0;
    virtual QString longRepresentation() const = 0;
    virtual bool canAppearMoreThanOnce() const { return false; }

    void parse(CommandType command, const QString &representation, QStringList &input);

protected:
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

} // namespace qbs

#endif // QBS_COMMANDLINEOPTION_H
