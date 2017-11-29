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

#ifndef QBS_EVALUATORSCRIPTCLASS_H
#define QBS_EVALUATORSCRIPTCLASS_H

#include "forward_decls.h"
#include "qualifiedid.h"

#include <tools/set.h>

#include <QtScript/qscriptclass.h>

#include <stack>

QT_BEGIN_NAMESPACE
class QScriptContext;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {
class EvaluationData;
class Item;
class PropertyDeclaration;
class ScriptEngine;

class EvaluatorScriptClass : public QScriptClass
{
public:
    EvaluatorScriptClass(ScriptEngine *scriptEngine);

    QueryFlags queryProperty(const QScriptValue &object,
                             const QScriptString &name,
                             QueryFlags flags, uint *id) override;
    QScriptValue property(const QScriptValue &object,
                          const QScriptString &name, uint id) override;
    QScriptClassPropertyIterator *newIterator(const QScriptValue &object) override;

    void setValueCacheEnabled(bool enabled);

    void convertToPropertyType(const PropertyDeclaration& decl, const CodeLocation &loc,
                               QScriptValue &v);

    PropertyDependencies propertyDependencies() const { return m_propertyDependencies; }
    void clearPropertyDependencies() { m_propertyDependencies.clear(); }

    void setPathPropertiesBaseDir(const QString &dirPath) { m_pathPropertiesBaseDir = dirPath; }
    void clearPathPropertiesBaseDir() { m_pathPropertiesBaseDir.clear(); }

private:
    QueryFlags queryItemProperty(const EvaluationData *data,
                                 const QString &name,
                                 bool ignoreParent = false);
    static QString resultToString(const QScriptValue &scriptValue);
    void collectValuesFromNextChain(const EvaluationData *data, QScriptValue *result, const QString &propertyName, const ValuePtr &value);

    void convertToPropertyType(const Item *item,
                               const PropertyDeclaration& decl, const Value *value,
                               QScriptValue &v);

    struct QueryResult
    {
        QueryResult()
            : data(nullptr), itemOfProperty(nullptr)
        {}

        bool isNull() const
        {
            static const QueryResult pristine;
            return *this == pristine;
        }

        bool operator==(const QueryResult &rhs) const
        {
            return foundInParent == rhs.foundInParent
                    && data == rhs.data
                    && itemOfProperty == rhs.itemOfProperty
                    && value == rhs.value;
        }

        bool foundInParent = false;
        const EvaluationData *data;
        const Item *itemOfProperty;     // The item that owns the property.
        ValuePtr value;
    };
    QueryResult m_queryResult;
    bool m_valueCacheEnabled;
    Set<Value *> m_currentNextChain;
    PropertyDependencies m_propertyDependencies;
    std::stack<QualifiedId> m_requestedProperties;
    QString m_pathPropertiesBaseDir;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_EVALUATORSCRIPTCLASS_H
