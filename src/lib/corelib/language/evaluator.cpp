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

#include "evaluator.h"

#include "evaluationdata.h"
#include "evaluatorscriptclass.h"
#include "filecontext.h"
#include "filetags.h"
#include "item.h"
#include "scriptengine.h"
#include "value.h"

#include <jsextensions/jsextensions.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/scripttools.h>
#include <tools/qbsassert.h>

#include <QtCore/qdebug.h>

namespace qbs {
namespace Internal {

Evaluator::Evaluator(ScriptEngine *scriptEngine)
    : m_scriptEngine(scriptEngine)
    , m_scriptClass(new EvaluatorScriptClass(scriptEngine, scriptEngine->logger()))
{
}

Evaluator::~Evaluator()
{
    for (auto it = m_scriptValueMap.cbegin(); it != m_scriptValueMap.cend(); ++it)
        delete attachedPointer<EvaluationData>(*it);
    delete m_scriptClass;
}

QScriptValue Evaluator::property(const Item *item, const QString &name)
{
    return scriptValue(item).property(name);
}

QScriptValue Evaluator::value(const Item *item, const QString &name, bool *propertyWasSet)
{
    QScriptValue v;
    evaluateProperty(&v, item, name, propertyWasSet);
    return v;
}

bool Evaluator::boolValue(const Item *item, const QString &name, bool defaultValue,
                          bool *propertyWasSet)
{
    QScriptValue v;
    if (!evaluateProperty(&v, item, name, propertyWasSet))
        return defaultValue;
    return v.toBool();
}

FileTags Evaluator::fileTagsValue(const Item *item, const QString &name, bool *propertySet)
{
    return FileTags::fromStringList(stringListValue(item, name, propertySet));
}

QString Evaluator::stringValue(const Item *item, const QString &name,
                               const QString &defaultValue, bool *propertyWasSet)
{
    QScriptValue v;
    if (!evaluateProperty(&v, item, name, propertyWasSet))
        return defaultValue;
    return v.toString();
}

static QStringList toStringList(const QScriptValue &scriptValue)
{
    if (scriptValue.isString()) {
        return QStringList(scriptValue.toString());
    } else if (scriptValue.isArray()) {
        QStringList lst;
        int i = 0;
        forever {
            QScriptValue elem = scriptValue.property(i++);
            if (!elem.isValid())
                break;
            lst.append(elem.toString());
        }
        return lst;
    }
    return QStringList();
}

QStringList Evaluator::stringListValue(const Item *item, const QString &name, bool *propertyWasSet)
{
    QScriptValue v = property(item, name);
    if (propertyWasSet)
        *propertyWasSet = v.isValid() && !v.isUndefined();
    handleEvaluationError(item, name, v);
    return toStringList(v);
}

QScriptValue Evaluator::scriptValue(const Item *item)
{
    QScriptValue &scriptValue = m_scriptValueMap[item];
    if (scriptValue.isObject()) {
        // already initialized
        return scriptValue;
    }

    EvaluationData *edata = new EvaluationData;
    edata->evaluator = this;
    edata->item = item;
    edata->item->setObserver(this);

    scriptValue = m_scriptEngine->newObject(m_scriptClass);
    attachPointerTo(scriptValue, edata);
    return scriptValue;
}

void Evaluator::onItemPropertyChanged(Item *item)
{
    EvaluationData *data = attachedPointer<EvaluationData>(m_scriptValueMap.value(item));
    if (data)
        data->valueCache.clear();
}

void Evaluator::handleEvaluationError(const Item *item, const QString &name,
        const QScriptValue &scriptValue)
{
    if (Q_LIKELY(!m_scriptEngine->hasErrorOrException(scriptValue)))
        return;
    QString message;
    QString filePath;
    int line = -1;
    const QScriptValue value = scriptValue.isError() ? scriptValue
                                                     : m_scriptEngine->uncaughtException();
    if (value.isError()) {
        QScriptValue v = value.property(QStringLiteral("message"));
        if (v.isString())
            message = v.toString();
        v = value.property(QStringLiteral("fileName"));
        if (v.isString())
            filePath = v.toString();
        v = value.property(QStringLiteral("lineNumber"));
        if (v.isNumber())
            line = v.toInt32();
    } else {
        message = value.toString();
        const ValueConstPtr value = item->property(name);
        if (value) {
            const CodeLocation location = value->location();
            filePath = location.filePath();
            line = location.line();
        }
    }
    throw ErrorInfo(message, CodeLocation(filePath, line, -1, false));
}

bool Evaluator::evaluateProperty(QScriptValue *result, const Item *item, const QString &name,
        bool *propertyWasSet)
{
    *result = property(item, name);
    handleEvaluationError(item, name, *result);
    if (!result->isValid() || result->isUndefined()) {
        if (propertyWasSet)
            *propertyWasSet = false;
        return false;
    }
    if (propertyWasSet)
        *propertyWasSet = true;
    return true;
}

Evaluator::FileContextScopes Evaluator::fileContextScopes(const FileContextConstPtr &file)
{
    FileContextScopes &result = m_fileContextScopesMap[file];
    if (!result.fileScope.isObject()) {
        if (file->idScope())
            result.fileScope = scriptValue(file->idScope());
        else
            result.fileScope = m_scriptEngine->newObject();
        result.fileScope.setProperty(QLatin1String("filePath"), file->filePath());
        result.fileScope.setProperty(QLatin1String("path"), file->dirPath());
    }
    if (!result.importScope.isObject()) {
        try {
            result.importScope = m_scriptEngine->newObject();
            m_scriptEngine->import(file, result.importScope);
            JsExtensions::setupExtensions(file->jsExtensions(), result.importScope);
        } catch (const ErrorInfo &e) {
            result.importScope = m_scriptEngine->currentContext()->throwError(e.toString());
        }
    }
    return result;
}

void Evaluator::setCachingEnabled(bool enabled)
{
    m_scriptClass->setValueCacheEnabled(enabled);
}

PropertyDependencies Evaluator::propertyDependencies() const
{
    return m_scriptClass->propertyDependencies();
}

void Evaluator::clearPropertyDependencies()
{
    m_scriptClass->clearPropertyDependencies();
}

} // namespace Internal
} // namespace qbs
