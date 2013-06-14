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
#include "rulesevaluationcontext.h"

#include "artifact.h"
#include "command.h"
#include "transformer.h"
#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/progressobserver.h>
#include <tools/qbsassert.h>

#include <QVariant>

namespace qbs {
namespace Internal {

RulesEvaluationContext::RulesEvaluationContext(const Logger &logger)
    : m_engine(new ScriptEngine(logger)), m_observer(0), m_initScopeCalls(0)
{
    m_prepareScriptScope = m_engine->newObject();
    ProcessCommand::setupForJavaScript(m_prepareScriptScope);
    JavaScriptCommand::setupForJavaScript(m_prepareScriptScope);
}

RulesEvaluationContext::~RulesEvaluationContext()
{
    delete m_engine;
}

void RulesEvaluationContext::initializeObserver(const QString &description, int maximumProgress)
{
    if (m_observer)
        m_observer->initialize(description, maximumProgress);
}

void RulesEvaluationContext::incrementProgressValue()
{
    if (m_observer)
        m_observer->incrementProgressValue();
}

void RulesEvaluationContext::checkForCancelation()
{
    if (Q_UNLIKELY(m_observer && m_observer->canceled()))
        throw ErrorInfo(Tr::tr("Build canceled."));
}

void RulesEvaluationContext::initScope()
{
    if (m_initScopeCalls++ > 0)
        return;

    m_engine->setProperty("lastSetupProject", QVariant());
    m_engine->setProperty("lastSetupProduct", QVariant());

    m_engine->clearImportsCache();
    m_engine->pushContext();
    m_scope = m_engine->newObject();
    m_scope.setPrototype(m_prepareScriptScope);
    m_engine->currentContext()->pushScope(m_scope);
}

void RulesEvaluationContext::cleanupScope()
{
    QBS_CHECK(m_initScopeCalls > 0);
    if (--m_initScopeCalls > 0)
        return;

    m_scope = QScriptValue();
    m_engine->currentContext()->popScope();
    m_engine->popContext();
}

RulesEvaluationContext::Scope::Scope(RulesEvaluationContext *evalContext)
    : m_evalContext(evalContext)
{
    evalContext->initScope();
}

RulesEvaluationContext::Scope::~Scope()
{
    m_evalContext->cleanupScope();
}

} // namespace Internal
} // namespace qbs
