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

#include "executorjob.h"

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
    connect(m_processCommandExecutor, SIGNAL(error(qbs::ErrorInfo)),
            this, SLOT(onCommandError(qbs::ErrorInfo)));
    connect(m_processCommandExecutor, SIGNAL(finished()), SLOT(onCommandFinished()));
    connect(m_jsCommandExecutor, SIGNAL(reportCommandDescription(QString,QString)),
            this, SIGNAL(reportCommandDescription(QString,QString)));
    connect(m_jsCommandExecutor, SIGNAL(error(qbs::ErrorInfo)),
            this, SLOT(onCommandError(qbs::ErrorInfo)));
    connect(m_jsCommandExecutor, SIGNAL(finished()), SLOT(onCommandFinished()));
    setInactive();
}

ExecutorJob::~ExecutorJob()
{
}

void ExecutorJob::setMainThreadScriptEngine(QScriptEngine *engine)
{
    m_processCommandExecutor->setMainThreadScriptEngine(engine);
    m_jsCommandExecutor->setMainThreadScriptEngine(engine);
}

void ExecutorJob::setDryRun(bool enabled)
{
    m_processCommandExecutor->setDryRunEnabled(enabled);
    m_jsCommandExecutor->setDryRunEnabled(enabled);
}

void ExecutorJob::run(Transformer *t, const ResolvedProductPtr &product)
{
    QBS_ASSERT(m_currentCommandIdx == -1, return);

    if (t->commands.isEmpty()) {
        emit success();
        return;
    }

    m_processCommandExecutor->setProcessEnvironment(product->buildEnvironment);

    m_transformer = t;
    runNextCommand();
}

void ExecutorJob::cancel()
{
    if (!m_transformer)
        return;
    m_currentCommandIdx = m_transformer->commands.count();
}

void ExecutorJob::waitForFinished()
{
    if (m_currentCommandExecutor)
        m_currentCommandExecutor->waitForFinished();
}

void ExecutorJob::runNextCommand()
{
    QBS_ASSERT(m_currentCommandIdx <= m_transformer->commands.count(), return);
    ++m_currentCommandIdx;
    if (m_currentCommandIdx >= m_transformer->commands.count()) {
        setInactive();
        emit success();
        return;
    }

    const AbstractCommand * const command = m_transformer->commands.at(m_currentCommandIdx);
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

    m_currentCommandExecutor->start(m_transformer, command);
}

void ExecutorJob::onCommandError(const ErrorInfo &err)
{
    setInactive();
    emit error(err);
}

void ExecutorJob::onCommandFinished()
{
    if (!m_transformer)
        return;
    runNextCommand();
}

void ExecutorJob::setInactive()
{
    m_transformer = 0;
    m_currentCommandExecutor = 0;
    m_currentCommandIdx = -1;
}

} // namespace Internal
} // namespace qbs
