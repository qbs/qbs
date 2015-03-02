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

#include "executorjob.h"

#include "artifact.h"
#include "command.h"
#include "jscommandexecutor.h"
#include "processcommandexecutor.h"
#include "transformer.h"
#include <language/language.h>
#include <tools/error.h>
#include <tools/qbsassert.h>

#include <QThread>

namespace qbs {
namespace Internal {

ExecutorJob::ExecutorJob(const Logger &logger, QObject *parent)
    : QObject(parent)
    , m_processCommandExecutor(new ProcessCommandExecutor(logger, this))
    , m_jsCommandExecutor(new JsCommandExecutor(logger, this))
{
    connect(m_processCommandExecutor, SIGNAL(reportCommandDescription(QString,QString)),
            this, SIGNAL(reportCommandDescription(QString,QString)));
    connect(m_processCommandExecutor, SIGNAL(reportProcessResult(qbs::ProcessResult)),
            this, SIGNAL(reportProcessResult(qbs::ProcessResult)));
    connect(m_processCommandExecutor, SIGNAL(finished(qbs::ErrorInfo)),
            this, SLOT(onCommandFinished(qbs::ErrorInfo)));
    connect(m_jsCommandExecutor, SIGNAL(reportCommandDescription(QString,QString)),
            this, SIGNAL(reportCommandDescription(QString,QString)));
    connect(m_jsCommandExecutor, SIGNAL(finished(qbs::ErrorInfo)),
            this, SLOT(onCommandFinished(qbs::ErrorInfo)));
    reset();
}

ExecutorJob::~ExecutorJob()
{
}

void ExecutorJob::setMainThreadScriptEngine(ScriptEngine *engine)
{
    m_processCommandExecutor->setMainThreadScriptEngine(engine);
    m_jsCommandExecutor->setMainThreadScriptEngine(engine);
}

void ExecutorJob::setDryRun(bool enabled)
{
    m_processCommandExecutor->setDryRunEnabled(enabled);
    m_jsCommandExecutor->setDryRunEnabled(enabled);
}

void ExecutorJob::setEchoMode(CommandEchoMode echoMode)
{
    m_processCommandExecutor->setEchoMode(echoMode);
    m_jsCommandExecutor->setEchoMode(echoMode);
}

void ExecutorJob::run(Transformer *t)
{
    QBS_ASSERT(m_currentCommandIdx == -1, return);

    if (t->commands.isEmpty()) {
        setFinished();
        return;
    }

    t->propertiesRequestedInCommands.clear();
    QBS_CHECK(!t->outputs.isEmpty());
    m_processCommandExecutor->setProcessEnvironment(
                (*t->outputs.begin())->product->buildEnvironment);
    m_transformer = t;
    runNextCommand();
}

void ExecutorJob::cancel()
{
    if (!m_currentCommandExecutor)
        return;
    m_error = ErrorInfo(tr("Transformer execution canceled."));
    m_currentCommandExecutor->cancel();
}

void ExecutorJob::runNextCommand()
{
    QBS_ASSERT(m_currentCommandIdx <= m_transformer->commands.count(), return);
    ++m_currentCommandIdx;
    if (m_currentCommandIdx >= m_transformer->commands.count()) {
        setFinished();
        return;
    }

    const AbstractCommandPtr &command = m_transformer->commands.at(m_currentCommandIdx);
    switch (command->type()) {
    case AbstractCommand::ProcessCommandType:
        m_currentCommandExecutor = m_processCommandExecutor;
        break;
    case AbstractCommand::JavaScriptCommandType:
        m_currentCommandExecutor = m_jsCommandExecutor;
        break;
    default:
        qFatal("Missing implementation for command type %d", command->type());
    }

    m_currentCommandExecutor->start(m_transformer, command.data());
}

void ExecutorJob::onCommandFinished(const ErrorInfo &err)
{
    QBS_ASSERT(m_transformer, return);
    if (m_error.hasError()) { // Canceled?
        setFinished();
    } else if (err.hasError()) {
        m_error = err;
        setFinished();
    } else {
        runNextCommand();
    }
}

void ExecutorJob::setFinished()
{
    const ErrorInfo err = m_error;
    reset();
    emit finished(err);
}

void ExecutorJob::reset()
{
    m_transformer = 0;
    m_currentCommandExecutor = 0;
    m_currentCommandIdx = -1;
    m_error.clear();
}

} // namespace Internal
} // namespace qbs
