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

#include "executorjob.h"

#include "artifact.h"
#include "jscommandexecutor.h"
#include "processcommandexecutor.h"
#include "rulecommands.h"
#include "transformer.h"
#include <language/language.h>
#include <tools/error.h>
#include <tools/qbsassert.h>

#include <QtCore/qthread.h>

namespace qbs {
namespace Internal {

ExecutorJob::ExecutorJob(const Logger &logger, QObject *parent)
    : QObject(parent)
    , m_processCommandExecutor(new ProcessCommandExecutor(logger, this))
    , m_jsCommandExecutor(new JsCommandExecutor(logger, this))
{
    connect(m_processCommandExecutor, &AbstractCommandExecutor::reportCommandDescription,
            this, &ExecutorJob::reportCommandDescription);
    connect(m_processCommandExecutor, &ProcessCommandExecutor::reportProcessResult,
            this, &ExecutorJob::reportProcessResult);
    connect(m_processCommandExecutor, &AbstractCommandExecutor::finished,
            this, &ExecutorJob::onCommandFinished);
    connect(m_jsCommandExecutor, &AbstractCommandExecutor::reportCommandDescription,
            this, &ExecutorJob::reportCommandDescription);
    connect(m_jsCommandExecutor, &AbstractCommandExecutor::finished,
            this, &ExecutorJob::onCommandFinished);
    reset();
}

ExecutorJob::~ExecutorJob() = default;

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

    if (t->commands.empty()) {
        setFinished();
        return;
    }

    t->propertiesRequestedInCommands.clear();
    t->propertiesRequestedFromArtifactInCommands.clear();
    t->importedFilesUsedInCommands.clear();
    t->depsRequestedInCommands.clear();
    t->artifactsMapRequestedInCommands.clear();
    t->exportedModulesAccessedInCommands.clear();
    t->lastCommandExecutionTime = FileTime::currentTime();
    QBS_CHECK(!t->outputs.empty());
    m_processCommandExecutor->setProcessEnvironment(
                (*t->outputs.cbegin())->product->buildEnvironment);
    m_transformer = t;
    m_jobPools = t->jobPools();
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
    QBS_ASSERT(m_currentCommandIdx <= m_transformer->commands.size(), return);
    ++m_currentCommandIdx;
    if (m_currentCommandIdx >= m_transformer->commands.size()) {
        setFinished();
        return;
    }

    const AbstractCommandPtr &command = m_transformer->commands.commandAt(m_currentCommandIdx);
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

    m_currentCommandExecutor->start(m_transformer, command.get());
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
    m_transformer = nullptr;
    m_jobPools.clear();
    m_currentCommandExecutor = nullptr;
    m_currentCommandIdx = -1;
    m_error.clear();
}

} // namespace Internal
} // namespace qbs
