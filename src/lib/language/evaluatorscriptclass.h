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

#ifndef QBS_EVALUATORSCRIPTCLASS_H
#define QBS_EVALUATORSCRIPTCLASS_H

#include "value.h"
#include "builtinvalue.h"
#include <logging/logger.h>

#include <QScriptClass>

QT_BEGIN_NAMESPACE
class QScriptContext;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {

class EvaluationData;

class EvaluatorScriptClass : public QScriptClass
{
public:
    EvaluatorScriptClass(QScriptEngine *scriptEngine, const Logger &logger);

    QueryFlags queryProperty(const QScriptValue &object,
                             const QScriptString &name,
                             QueryFlags flags, uint *id);
    QScriptValue property(const QScriptValue &object,
                          const QScriptString &name, uint id);

    QScriptValue scriptValueForBuiltin(BuiltinValue::Builtin builtin) const;

private:
    QueryFlags queryItemProperty(const EvaluationData *data,
                                 const QString &name,
                                 bool ignoreParent = false);
    static QString resultToString(const QScriptValue &scriptValue);
    static Item *findParentOfType(const Item *item, const QString &typeName);
    static QScriptValue js_getenv(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_getHostOS(QScriptContext *context, QScriptEngine *engine);

    struct QueryResult
    {
        QueryResult()
            : data(0), inPrototype(false)
        {}

        bool isNull() const
        {
            return !data;
        }

        const EvaluationData *data;
        bool inPrototype;
        ValuePtr value;
    };
    QueryResult m_queryResult;
    Logger m_logger;
    QScriptValue m_getenvBuiltin;
    QScriptValue m_getHostOSBuiltin;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_EVALUATORSCRIPTCLASS_H
