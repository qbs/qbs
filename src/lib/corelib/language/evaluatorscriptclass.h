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

#ifndef QBS_EVALUATORSCRIPTCLASS_H
#define QBS_EVALUATORSCRIPTCLASS_H

#include "builtinvalue.h"
#include "forward_decls.h"

#include <logging/logger.h>

#include <QScriptClass>
#include <QStack>

QT_BEGIN_NAMESPACE
class QScriptContext;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {
class EvaluationData;
class ScriptEngine;

class EvaluatorScriptClass : public QScriptClass
{
public:
    EvaluatorScriptClass(ScriptEngine *scriptEngine, const Logger &logger);

    QueryFlags queryProperty(const QScriptValue &object,
                             const QScriptString &name,
                             QueryFlags flags, uint *id);
    QScriptValue property(const QScriptValue &object,
                          const QScriptString &name, uint id);

    void setValueCacheEnabled(bool enabled);
    QScriptValue scriptValueForBuiltin(BuiltinValue::Builtin builtin) const;

    static QScriptValue js_getNativeSetting(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_getEnv(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_currentEnv(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_canonicalArchitecture(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_rfc1034identifier(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_getHash(QScriptContext *context, QScriptEngine *engine);

private:
    QueryFlags queryItemProperty(const EvaluationData *data,
                                 const QString &name,
                                 bool ignoreParent = false);
    static QString resultToString(const QScriptValue &scriptValue);
    static Item *findParentOfType(const Item *item, const QString &typeName);
    void collectValuesFromNextChain(const EvaluationData *data, QScriptValue *result, const QString &propertyName, const ValuePtr &value);

    struct QueryResult
    {
        QueryResult()
            : data(0), itemOfProperty(0)
        {}

        bool isNull() const
        {
            return !data;
        }

        const EvaluationData *data;
        const Item *itemOfProperty;     // The item that owns the property.
        ValuePtr value;
    };
    QueryResult m_queryResult;
    Logger m_logger;
    bool m_valueCacheEnabled;
    QScriptValue m_getNativeSettingBuiltin;
    QScriptValue m_getEnvBuiltin;
    QScriptValue m_currentEnvBuiltin;
    QScriptValue m_canonicalArchitectureBuiltin;
    QScriptValue m_rfc1034identifierBuiltin;
    QScriptValue m_getHashBuiltin;
    QStack<JSSourceValue *> m_sourceValueStack;
    QSet<Value *> m_currentNextChain;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_EVALUATORSCRIPTCLASS_H
