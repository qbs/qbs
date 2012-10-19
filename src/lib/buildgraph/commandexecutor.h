/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#ifndef COMMANDEXECUTOR_H
#define COMMANDEXECUTOR_H

#include <QObject>
#include <QProcess>

QT_BEGIN_NAMESPACE
class QProcess;
class QScriptEngine;
QT_END_NAMESPACE

namespace qbs {

class AbstractCommand;
class ProcessCommand;
class JavaScriptCommand;
class JavaScriptCommandFutureWatcher;
class ResolvedProduct;
class Transformer;

class CommandExecutor : public QObject
{
    Q_OBJECT
public:
    explicit CommandExecutor(QObject *parent = 0);

    void setMainThreadScriptEngine(QScriptEngine *engine) { m_mainThreadScriptEngine = engine; }
    void setDryRunEnabled(bool enabled) { dryRun = enabled; }
    void setProcessEnvironment(const QProcessEnvironment &processEnvironment);
    void waitForFinished();

signals:
    void error(QString errorString);
    void finished();

public slots:
    void start(Transformer *transformer, AbstractCommand *cmd);

protected:
    void printCommandInfo(AbstractCommand *cmd);
    void startProcessCommand();
    QString filterProcessOutput(const QByteArray &output, const QString &filterFunctionSource);
    void sendProcessOutput(bool dueToError);
    void startJavaScriptCommand();

protected slots:
    void onProcessError(QProcess::ProcessError);
    void onProcessFinished(int exitCode);
    void onJavaScriptCommandFinished();

private:
    void removeResponseFile();
    QString findProcessCommandInPath();
    QString findProcessCommandBySuffix();
    bool findProcessCandidateCheck(const QString &directory, const QString &program, QString &fullProgramPath);

private:
    static const QStringList m_executableSuffixes;
    Transformer *m_transformer;
    bool dryRun;

    // members for executing ProcessCommand
    ProcessCommand *m_processCommand;
    QString m_commandLine;
    QProcess m_process;
    QString m_responseFileName;
    QScriptEngine *m_mainThreadScriptEngine;

    // members for executing JavaScriptCommand members
    JavaScriptCommand *m_jsCommand;
    JavaScriptCommandFutureWatcher *m_jsFutureWatcher;
};

} // namespace qbs

#endif // COMMANDEXECUTOR_H
