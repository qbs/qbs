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
#include "rulesevaluationcontext.h"

#include "artifact.h"
#include "rulecommands.h"
#include "transformer.h"
#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/progressobserver.h>
#include <tools/qbsassert.h>

#include <QtCore/qvariant.h>

namespace qbs {
namespace Internal {

RulesEvaluationContext::RulesEvaluationContext(Logger logger)
    : m_logger(std::move(logger)),
      m_engine(ScriptEngine::create(m_logger, EvalContext::RuleExecution)),
      m_observer(nullptr),
      m_initScopeCalls(0)
{
    m_prepareScriptScope = m_engine->newObject();
    m_prepareScriptScope.setPrototype(m_engine->globalObject());
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

    m_engine->setActive(true);
    m_scope = m_engine->newObject();
    m_scope.setPrototype(m_prepareScriptScope);
    m_engine->setGlobalObject(m_scope);
}

void RulesEvaluationContext::cleanupScope()
{
    QBS_CHECK(m_initScopeCalls > 0);
    if (--m_initScopeCalls > 0)
        return;

    m_scope = QScriptValue();
    m_engine->setGlobalObject(m_prepareScriptScope.prototype());
    m_engine->setActive(false);
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
