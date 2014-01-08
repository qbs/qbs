/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "evaluator.h"
#include "evaluationdata.h"
#include "evaluatorscriptclass.h"
#include "filecontext.h"
#include "filetags.h"
#include "item.h"
#include <jsextensions/jsextensions.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/qbsassert.h>
#include <QDebug>
#include <QScriptEngine>

namespace qbs {
namespace Internal {


Evaluator::Evaluator(ScriptEngine *scriptEngine, const Logger &logger)
    : m_scriptEngine(scriptEngine)
    , m_scriptClass(new EvaluatorScriptClass(scriptEngine, logger))
{
}

Evaluator::~Evaluator()
{
    QHash<const Item *, QScriptValue>::iterator it = m_scriptValueMap.begin();
    for (; it != m_scriptValueMap.end(); ++it) {
        EvaluationData *data = EvaluationData::get(*it);
        if (data) {
            if (data->item)
                data->item->setPropertyObserver(0);
            delete data;
        }
    }
    delete m_scriptClass;
}

QScriptValue Evaluator::property(const Item *item, const QString &name)
{
    return scriptValue(item).property(name);
}

QScriptValue Evaluator::property(const Item *item, const QStringList &nameParts)
{
    const Item *targetItem = item;
    const int c = nameParts.count() - 1;
    for (int i = 0; i < c; ++i) {
        ValuePtr v = targetItem->properties().value(nameParts.at(i));
        if (!v)
            return QScriptValue();
        QBS_ASSERT(v->type() == Value::ItemValueType, return QScriptValue());
        targetItem =  v.staticCast<ItemValue>()->item();
        QBS_ASSERT(targetItem, return QScriptValue());
    }
    return property(targetItem, nameParts.last());
}

bool Evaluator::boolValue(const Item *item, const QString &name, bool defaultValue,
                          bool *propertyWasSet)
{
    QScriptValue v = property(item, name);
    handleEvaluationError(item, name, v);
    if (!v.isValid() || v.isUndefined()) {
        if (propertyWasSet)
            *propertyWasSet = false;
        return defaultValue;
    }
    if (propertyWasSet)
        *propertyWasSet = true;
    return v.toBool();
}

FileTags Evaluator::fileTagsValue(const Item *item, const QString &name)
{
    return FileTags::fromStringList(stringListValue(item, name));
}

QString Evaluator::stringValue(const Item *item, const QString &name,
                               const QString &defaultValue, bool *propertyWasSet)
{
    QScriptValue v = property(item, name);
    handleEvaluationError(item, name, v);
    if (!v.isValid() || v.isUndefined()) {
        if (propertyWasSet)
            *propertyWasSet = false;
        return defaultValue;
    }
    if (propertyWasSet)
        *propertyWasSet = true;
    return v.toString();
}

static QStringList toStringList(const QScriptValue &scriptValue,
        const Item *item, const QString &propertyName)
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
            if (elem.isArray() || elem.isObject()) {
                // Let's assume all other JS types are convertible to string.
                throw ErrorInfo(Tr::tr("Expected array element of type String at index %1.")
                                .arg(i - 1),
                                item->property(propertyName)->location());
            }
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
    return toStringList(v, item, name);
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
    edata->item->setPropertyObserver(this);

    scriptValue = m_scriptEngine->newObject(m_scriptClass);
    edata->attachTo(scriptValue);
    return scriptValue;
}

void Evaluator::onItemPropertyChanged(Item *item)
{
    EvaluationData *data = EvaluationData::get(m_scriptValueMap.value(item));
    if (data)
        data->valueCache.clear();
}

void Evaluator::onItemDestroyed(Item *item)
{
    delete EvaluationData::get(m_scriptValueMap.value(item));
    m_scriptValueMap.remove(item);
}

void Evaluator::handleEvaluationError(const Item *item, const QString &name,
        const QScriptValue &scriptValue)
{
    if (Q_LIKELY(!scriptValue.isError() && !m_scriptEngine->hasUncaughtException()))
        return;
    const ValueConstPtr value = item->property(name);
    CodeLocation location = value ? value->location() : CodeLocation();
    if (m_scriptEngine->hasUncaughtException()) {
        throw ErrorInfo(m_scriptEngine->uncaughtException().toString(),
                CodeLocation(location.fileName(), m_scriptEngine->uncaughtExceptionLineNumber()));
    }
    throw ErrorInfo(scriptValue.toString(), location);
}

QScriptValue Evaluator::fileScope(const FileContextConstPtr &file)
{
    QScriptValue &result = m_fileScopeMap[file];
    if (result.isObject()) {
        // already initialized
        return result;
    }

    if (file->idScope())
        result = scriptValue(file->idScope());
    else
        result = m_scriptEngine->newObject();
    result.setProperty(QLatin1String("filePath"), file->filePath());
    result.setProperty(QLatin1String("path"), file->dirPath());
    m_scriptEngine->import(file->jsImports(), result, result);
    JsExtensions::setupExtensions(file->jsExtensions(), result);
    return result;
}

} // namespace Internal
} // namespace qbs
