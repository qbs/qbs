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

#ifndef QBS_ABSTRACTCOMMANDEXECUTOR_H
#define QBS_ABSTRACTCOMMANDEXECUTOR_H

#include <logging/logger.h>
#include <tools/commandechomode.h>
#include <tools/error.h>

#include <QObject>

namespace qbs {
class ErrorInfo;

namespace Internal {
class AbstractCommand;
class ScriptEngine;
class Transformer;

class AbstractCommandExecutor : public QObject
{
    Q_OBJECT
public:
    explicit AbstractCommandExecutor(const Internal::Logger &logger, QObject *parent = 0);

    void setMainThreadScriptEngine(ScriptEngine *engine) { m_mainThreadScriptEngine = engine; }
    void setDryRunEnabled(bool enabled) { m_dryRun = enabled; }
    void setEchoMode(CommandEchoMode echoMode) { m_echoMode = echoMode; }

    virtual void cancel() = 0;

public slots:
    void start(Transformer *transformer, const AbstractCommand *cmd);

signals:
    void reportCommandDescription(const QString &highlight, const QString &message);
    void finished(const qbs::ErrorInfo &err = ErrorInfo()); // !hasError() <=> command successful

protected:
    virtual void doReportCommandDescription();
    const AbstractCommand *command() const { return m_command; }
    Transformer *transformer() const { return m_transformer; }
    ScriptEngine *scriptEngine() const { return m_mainThreadScriptEngine; }
    bool dryRun() const { return m_dryRun; }
    Internal::Logger logger() const { return m_logger; }
    CommandEchoMode m_echoMode;

private:
    virtual void doSetup() { };
    virtual void doStart() = 0;

private:
    const AbstractCommand *m_command;
    Transformer *m_transformer;
    ScriptEngine *m_mainThreadScriptEngine;
    bool m_dryRun;
    Internal::Logger m_logger;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_ABSTRACTCOMMANDEXECUTOR_H
