/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/
#ifndef COMMAND_H
#define COMMAND_H

#include <tools/codelocation.h>

#include <QStringList>
#include <QVariantMap>

QT_BEGIN_NAMESPACE
class QScriptEngine;
class QScriptValue;
QT_END_NAMESPACE

namespace qbs {

class AbstractCommand
{
public:
    AbstractCommand();
    virtual ~AbstractCommand();

    enum CommandType {
        AbstractCommandType,
        ProcessCommandType,
        JavaScriptCommandType
    };

    static AbstractCommand *createByType(CommandType commandType);

    virtual CommandType type() const { return AbstractCommandType; }
    virtual void fillFromScriptValue(const QScriptValue *scriptValue, const CodeLocation &codeLocation);
    virtual void load(QDataStream &s);
    virtual void store(QDataStream &s);

    const QString description() const { return m_description; }
    void setDescription(const QString &str) { m_description = str; }

    const QString highlight() const { return m_highlight; }
    void setHighlight(const QString &str) { m_highlight = str; }

    bool isSilent() const { return m_silent; }
    void setSilent(bool b) { m_silent = b; }

private:
    QString m_description;
    QString m_highlight;
    bool m_silent;
};

class ProcessCommand : public AbstractCommand
{
public:
    static void setupForJavaScript(QScriptEngine *engine);

    ProcessCommand();

    CommandType type() const { return ProcessCommandType; }
    void fillFromScriptValue(const QScriptValue *scriptValue, const CodeLocation &codeLocation);
    void load(QDataStream &s);
    void store(QDataStream &s);

    const QString program() const { return m_program; }
    void setProgram(const QString &str) { m_program = str; }

    const QStringList arguments() const { return m_arguments; }
    void setArguments(const QStringList &l) { m_arguments = l; }

    const QString workingDir() const { return m_workingDir; }
    void setWorkingDir(const QString &str) { m_workingDir = str; }

    int maxExitCode() const { return m_maxExitCode; }
    void setMaxExitCode(int n) { m_maxExitCode = n; }

    QString stdoutFilterFunction() const { return m_stdoutFilterFunction; }
    void setStdoutFilterFunction(const QString &filter) { m_stdoutFilterFunction = filter; }

    QString stderrFilterFunction() const { return m_stderrFilterFunction; }
    void setStderrFilterFunction(const QString &filter) { m_stderrFilterFunction = filter; }

    int responseFileThreshold() const { return m_responseFileThreshold; }
    void setResponseFileThreshold(int n) { m_responseFileThreshold = n; }

    QString responseFileUsagePrefix() const { return m_responseFileUsagePrefix; }
    void setResponseFileUsagePrefix(const QString &function) { m_responseFileUsagePrefix = function; }

private:
    QString m_program;
    QStringList m_arguments;
    QString m_workingDir;
    int m_maxExitCode;
    QString m_stdoutFilterFunction;
    QString m_stderrFilterFunction;
    int m_responseFileThreshold; // When to use response files? In bytes of (program name + arguments).
    QString m_responseFileUsagePrefix;
};

class JavaScriptCommand : public AbstractCommand
{
public:
    static void setupForJavaScript(QScriptEngine *engine);

    JavaScriptCommand();

    virtual CommandType type() const { return JavaScriptCommandType; }
    void fillFromScriptValue(const QScriptValue *scriptValue, const CodeLocation &codeLocation);
    void load(QDataStream &s);
    void store(QDataStream &s);

    const QString &sourceCode() const { return m_sourceCode; }
    void setSourceCode(const QString &str) { m_sourceCode = str; }
    const CodeLocation &codeLocation() const { return m_codeLocation; }
    void setCodeLocation(const CodeLocation &loc) { m_codeLocation = loc; }
    const QVariantMap &properties() const { return m_properties; }

private:
    QString m_sourceCode;
    QVariantMap m_properties;
    CodeLocation m_codeLocation;
};

} // namespace qbs

#endif // COMMAND_H
