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
    QByteArray filterProcessOutput(const QByteArray &output, const QString &filterFunctionSource);
    void sendProcessOutput(bool logCommandLine = false);
    void startJavaScriptCommand();

protected slots:
    void onProcessError(QProcess::ProcessError);
    void onProcessFinished(int exitCode);
    void onJavaScriptCommandFinished();

private:
    void removeResponseFile();

private:
    Transformer *m_transformer;
    bool dryRun;

    // members for executing ProcessCommand
    ProcessCommand *m_processCommand;
    QProcess m_process;
    QString m_responseFileName;
    QScriptEngine *m_mainThreadScriptEngine;

    // members for executing JavaScriptCommand members
    JavaScriptCommand *m_jsCommand;
    JavaScriptCommandFutureWatcher *m_jsFutureWatcher;
};

} // namespace qbs

#endif // COMMANDEXECUTOR_H
