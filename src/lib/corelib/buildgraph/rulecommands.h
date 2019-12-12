/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2019 Jochen Ulrich <jochenulrich@t-online.de>
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

#ifndef QBS_BUILDGRAPH_COMMAND_H
#define QBS_BUILDGRAPH_COMMAND_H

#include "forward_decls.h"

#include <tools/codelocation.h>
#include <tools/persistence.h>
#include <tools/set.h>

#include <QtCore/qprocess.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

#include <QtScript/qscriptvalue.h>

namespace qbs {
namespace Internal {

class AbstractCommand
{
public:
    virtual ~AbstractCommand();

    enum CommandType {
        ProcessCommandType,
        JavaScriptCommandType
    };

    static QString defaultDescription() { return {}; }
    static QString defaultExtendedDescription() { return {}; }
    static QString defaultHighLight() { return {}; }
    static bool defaultIgnoreDryRun() { return false; }
    static bool defaultIsSilent() { return false; }
    static int defaultTimeout() { return -1; }

    virtual CommandType type() const = 0;
    virtual bool equals(const AbstractCommand *other) const;
    virtual void fillFromScriptValue(const QScriptValue *scriptValue, const CodeLocation &codeLocation);

    QString fullDescription(const QString &productName) const;
    const QString description() const { return m_description; }
    const QString extendedDescription() const { return m_extendedDescription; }
    const QString highlight() const { return m_highlight; }
    bool ignoreDryRun() const { return m_ignoreDryRun; }
    bool isSilent() const { return m_silent; }
    QString jobPool() const { return m_jobPool; }
    CodeLocation codeLocation() const { return m_codeLocation; }
    int timeout() const { return m_timeout; }

    const QVariantMap &properties() const { return m_properties; }

    virtual void load(PersistentPool &pool);
    virtual void store(PersistentPool &pool);

protected:
    AbstractCommand();
    void applyCommandProperties(const QScriptValue *scriptValue);

    Set<QString> m_predefinedProperties;

private:
    template<PersistentPool::OpType opType> void serializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(m_description, m_extendedDescription, m_highlight,
                                     m_ignoreDryRun, m_silent, m_codeLocation, m_jobPool,
                                     m_timeout, m_properties);
    }

    QString m_description;
    QString m_extendedDescription;
    QString m_highlight;
    bool m_ignoreDryRun;
    bool m_silent;
    CodeLocation m_codeLocation;
    QString m_jobPool;
    int m_timeout;
    QVariantMap m_properties;
};

class ProcessCommand : public AbstractCommand
{
public:
    static ProcessCommandPtr create() { return ProcessCommandPtr(new ProcessCommand); }
    static void setupForJavaScript(QScriptValue targetObject);

    CommandType type() const override { return ProcessCommandType; }
    bool equals(const AbstractCommand *otherAbstractCommand) const override;
    void fillFromScriptValue(const QScriptValue *scriptValue,
                             const CodeLocation &codeLocation) override;
    const QString program() const { return m_program; }
    const QStringList arguments() const { return m_arguments; }
    const QString workingDir() const { return m_workingDir; }
    int maxExitCode() const { return m_maxExitCode; }
    QString stdoutFilterFunction() const { return m_stdoutFilterFunction; }
    QString stderrFilterFunction() const { return m_stderrFilterFunction; }
    int responseFileThreshold() const { return m_responseFileThreshold; }
    int responseFileArgumentIndex() const { return m_responseFileArgumentIndex; }
    QString responseFileUsagePrefix() const { return m_responseFileUsagePrefix; }
    QString responseFileSeparator() const { return m_responseFileSeparator; }
    QProcessEnvironment environment() const { return m_environment; }
    QStringList relevantEnvVars() const;
    void clearRelevantEnvValues() { m_relevantEnvValues.clear(); }
    void addRelevantEnvValue(const QString &key, const QString &value);
    QString relevantEnvValue(const QString &key) const { return m_relevantEnvValues.value(key); }
    QString stdoutFilePath() const { return m_stdoutFilePath; }
    QString stderrFilePath() const { return m_stderrFilePath; }

    void load(PersistentPool &pool) override;
    void store(PersistentPool &pool) override;

private:
    ProcessCommand();

    int defaultResponseFileThreshold() const;

    void getEnvironmentFromList(const QStringList &envList);

    template<PersistentPool::OpType opType> void serializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(m_program, m_arguments, m_environment, m_workingDir,
                                     m_stdoutFilterFunction, m_stderrFilterFunction,
                                     m_responseFileUsagePrefix, m_responseFileSeparator,
                                     m_maxExitCode, m_responseFileThreshold,
                                     m_responseFileArgumentIndex, m_relevantEnvVars,
                                     m_relevantEnvValues, m_stdoutFilePath, m_stderrFilePath);
    }

    QString m_program;
    QStringList m_arguments;
    QString m_workingDir;
    int m_maxExitCode;
    QString m_stdoutFilterFunction;
    QString m_stderrFilterFunction;
    int m_responseFileThreshold; // When to use response files? In bytes of (program name + arguments).
    int m_responseFileArgumentIndex;
    QString m_responseFileUsagePrefix;
    QString m_responseFileSeparator;
    QProcessEnvironment m_environment;
    QStringList m_relevantEnvVars;
    QProcessEnvironment m_relevantEnvValues;
    QString m_stdoutFilePath;
    QString m_stderrFilePath;
};

class JavaScriptCommand : public AbstractCommand
{
public:
    static JavaScriptCommandPtr create() { return JavaScriptCommandPtr(new JavaScriptCommand); }
    static void setupForJavaScript(QScriptValue targetObject);

    CommandType type() const override { return JavaScriptCommandType; }
    bool equals(const AbstractCommand *otherAbstractCommand) const override;
    void fillFromScriptValue(const QScriptValue *scriptValue,
                             const CodeLocation &codeLocation) override;

    const QString &scopeName() const { return m_scopeName; }
    const QString &sourceCode() const { return m_sourceCode; }
    void setSourceCode(const QString &str) { m_sourceCode = str; }

    void load(PersistentPool &pool) override;
    void store(PersistentPool &pool) override;

private:
    JavaScriptCommand();

    template<PersistentPool::OpType opType> void serializationOp(PersistentPool &pool)
    {
        pool.serializationOp<opType>(m_scopeName, m_sourceCode);
    }

    QString m_scopeName;
    QString m_sourceCode;
};

class CommandList
{
public:
    bool empty() const { return m_commands.empty(); }
    int size() const { return m_commands.size(); }
    AbstractCommandPtr commandAt(int i) const { return m_commands.at(i); }
    const QList<AbstractCommandPtr> &commands() const { return m_commands; }

    void clear() { m_commands.clear(); }
    void addCommand(const AbstractCommandPtr &cmd) { m_commands.push_back(cmd); }

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
private:
    QList<AbstractCommandPtr> m_commands;
};
bool operator==(const CommandList &cl1, const CommandList &cl2);
inline bool operator!=(const CommandList &cl1, const CommandList &cl2) { return !(cl1 == cl2); }

} // namespace Internal
} // namespace qbs

#endif // QBS_BUILDGRAPH_COMMAND_H
