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

#ifndef QBS_ABSTRACTCOMMANDEXECUTOR_H
#define QBS_ABSTRACTCOMMANDEXECUTOR_H

#include <logging/logger.h>
#include <tools/commandechomode.h>
#include <tools/error.h>

#include <QtCore/qobject.h>
#include <QtCore/qtimer.h>

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
    explicit AbstractCommandExecutor(Internal::Logger logger, QObject *parent = nullptr);

    void setMainThreadScriptEngine(ScriptEngine *engine) { m_mainThreadScriptEngine = engine; }
    void setDryRunEnabled(bool enabled) { m_dryRun = enabled; }
    void setEchoMode(CommandEchoMode echoMode) { m_echoMode = echoMode; }

    virtual void cancel(const qbs::ErrorInfo &reason = {}) = 0;

    void start(Transformer *transformer, AbstractCommand *cmd);

signals:
    void reportCommandDescription(const QString &highlight, const QString &message);
    void finished(const qbs::ErrorInfo &err = ErrorInfo()); // !hasError() <=> command successful

protected:
    virtual void doReportCommandDescription(const QString &productName);
    AbstractCommand *command() const { return m_command; }
    Transformer *transformer() const { return m_transformer; }
    ScriptEngine *scriptEngine() const { return m_mainThreadScriptEngine; }
    bool dryRun() const { return m_dryRun; }
    Internal::Logger logger() const { return m_logger; }
    CommandEchoMode m_echoMode;

private:
    virtual void doSetup() { };
    virtual bool doStart() = 0;

    void startTimeout();

private:
    AbstractCommand *m_command;
    Transformer *m_transformer;
    ScriptEngine *m_mainThreadScriptEngine;
    bool m_dryRun;
    Internal::Logger m_logger;
    QTimer m_watchdog;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_ABSTRACTCOMMANDEXECUTOR_H
