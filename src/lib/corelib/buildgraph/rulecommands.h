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

#ifndef QBS_BUILDGRAPH_COMMAND_H
#define QBS_BUILDGRAPH_COMMAND_H

#include "forward_decls.h"

#include <tools/codelocation.h>
#include <tools/persistentobject.h>
#include <tools/set.h>

#include <QtCore/qprocess.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

#include <QtScript/qscriptvalue.h>

namespace qbs {
namespace Internal {

class AbstractCommand : public PersistentObject
{
public:
    virtual ~AbstractCommand();

    enum CommandType {
        ProcessCommandType,
        JavaScriptCommandType
    };

    static QString defaultDescription() { return QString(); }
    static QString defaultExtendedDescription() { return QString(); }
    static QString defaultHighLight() { return QString(); }
    static bool defaultIgnoreDryRun() { return false; }
    static bool defaultIsSilent() { return false; }

    virtual CommandType type() const = 0;
    virtual bool equals(const AbstractCommand *other) const;
    virtual void fillFromScriptValue(const QScriptValue *scriptValue, const CodeLocation &codeLocation);

    const QString description() const { return m_description; }
    const QString extendedDescription() const { return m_extendedDescription; }
    const QString highlight() const { return m_highlight; }
    bool ignoreDryRun() const { return m_ignoreDryRun; }
    bool isSilent() const { return m_silent; }
    CodeLocation codeLocation() const { return m_codeLocation; }

    const QVariantMap &properties() const { return m_properties; }

protected:
    AbstractCommand();

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;
    void applyCommandProperties(const QScriptValue *scriptValue);

    Set<QString> m_predefinedProperties;

private:
    QString m_description;
    QString m_extendedDescription;
    QString m_highlight;
    bool m_ignoreDryRun;
    bool m_silent;
    CodeLocation m_codeLocation;
    QVariantMap m_properties;
};

class ProcessCommand : public AbstractCommand
{
public:
    static ProcessCommandPtr create() { return ProcessCommandPtr(new ProcessCommand); }
    static void setupForJavaScript(QScriptValue targetObject);

    CommandType type() const { return ProcessCommandType; }
    bool equals(const AbstractCommand *otherAbstractCommand) const;
    void fillFromScriptValue(const QScriptValue *scriptValue, const CodeLocation &codeLocation);
    const QString program() const { return m_program; }
    const QStringList arguments() const { return m_arguments; }
    const QString workingDir() const { return m_workingDir; }
    int maxExitCode() const { return m_maxExitCode; }
    QString stdoutFilterFunction() const { return m_stdoutFilterFunction; }
    QString stderrFilterFunction() const { return m_stderrFilterFunction; }
    int responseFileThreshold() const { return m_responseFileThreshold; }
    int responseFileArgumentIndex() const { return m_responseFileArgumentIndex; }
    QString responseFileUsagePrefix() const { return m_responseFileUsagePrefix; }
    QProcessEnvironment environment() const { return m_environment; }
    QStringList relevantEnvVars() const;
    QString stdoutFilePath() const { return m_stdoutFilePath; }
    QString stderrFilePath() const { return m_stderrFilePath; }

private:
    ProcessCommand();

    int defaultResponseFileThreshold() const;

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

    void getEnvironmentFromList(const QStringList &envList);

    QString m_program;
    QStringList m_arguments;
    QString m_workingDir;
    int m_maxExitCode;
    QString m_stdoutFilterFunction;
    QString m_stderrFilterFunction;
    int m_responseFileThreshold; // When to use response files? In bytes of (program name + arguments).
    int m_responseFileArgumentIndex;
    QString m_responseFileUsagePrefix;
    QProcessEnvironment m_environment;
    QStringList m_relevantEnvVars;
    QString m_stdoutFilePath;
    QString m_stderrFilePath;
};

class JavaScriptCommand : public AbstractCommand
{
public:
    static JavaScriptCommandPtr create() { return JavaScriptCommandPtr(new JavaScriptCommand); }
    static void setupForJavaScript(QScriptValue targetObject);

    virtual CommandType type() const { return JavaScriptCommandType; }
    bool equals(const AbstractCommand *otherAbstractCommand) const;
    void fillFromScriptValue(const QScriptValue *scriptValue, const CodeLocation &codeLocation);

    const QString &scopeName() const { return m_scopeName; }
    const QString &sourceCode() const { return m_sourceCode; }
    void setSourceCode(const QString &str) { m_sourceCode = str; }

private:
    JavaScriptCommand();

    void load(PersistentPool &pool);
    void store(PersistentPool &pool) const;

    QString m_scopeName;
    QString m_sourceCode;
};

QList<AbstractCommandPtr> loadCommandList(PersistentPool &pool);
void storeCommandList(const QList<AbstractCommandPtr> &commands, PersistentPool &pool);

bool commandListsAreEqual(const QList<AbstractCommandPtr> &l1, const QList<AbstractCommandPtr> &l2);

} // namespace Internal
} // namespace qbs

#endif // QBS_BUILDGRAPH_COMMAND_H
