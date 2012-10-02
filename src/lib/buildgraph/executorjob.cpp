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

#include "executorjob.h"
#include "executor.h"
#include "commandexecutor.h"
#include <buildgraph/transformer.h>
#include <QThread>

namespace qbs {

ExecutorJob::ExecutorJob(Executor *parent)
    : QObject(parent)
    , m_executor(parent)
    , m_commandExecutor(new CommandExecutor(this))
    , m_transformer(0)
    , m_lastUsedProduct(0)
    , m_currentCommandIdx(-1)
{
    connect(m_commandExecutor, SIGNAL(error(QString)), SLOT(onCommandError(QString)));
    connect(m_commandExecutor, SIGNAL(finished()), SLOT(onCommandFinished()));
}

ExecutorJob::~ExecutorJob()
{
}

void ExecutorJob::setMainThreadScriptEngine(QScriptEngine *engine)
{
    m_commandExecutor->setMainThreadScriptEngine(engine);
}

void ExecutorJob::setDryRun(bool enabled)
{
    m_commandExecutor->setDryRunEnabled(enabled);
}

void ExecutorJob::run(Transformer *t, BuildProduct *buildProduct)
{
    Q_ASSERT(m_currentCommandIdx == -1);

    if (t->commands.isEmpty()) {
        emit success();
        return;
    }

    if (m_lastUsedProduct != buildProduct)
        m_commandExecutor->setProcessEnvironment(buildProduct->rProduct->buildEnvironment);

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
    m_commandExecutor->waitForFinished();
}

void ExecutorJob::runNextCommand()
{
    ++m_currentCommandIdx;
    if (m_currentCommandIdx >= m_transformer->commands.count()) {
        m_transformer = 0;
        m_currentCommandIdx = -1;
        emit success();
        return;
    }

    AbstractCommand *command = m_transformer->commands.at(m_currentCommandIdx);
    m_commandExecutor->start(m_transformer, command);
}

void ExecutorJob::onCommandError(QString errorString)
{
    m_transformer = 0;
    m_currentCommandIdx = -1;
    emit error(errorString);
}

void ExecutorJob::onCommandFinished()
{
    if (!m_transformer)
        return;
    runNextCommand();
}

} // namespace qbs
