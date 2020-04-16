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

#ifndef QBS_EVALUATOR_H
#define QBS_EVALUATOR_H

#include "forward_decls.h"
#include "itemobserver.h"
#include "qualifiedid.h"

#include <QtCore/qhash.h>

#include <QtScript/qscriptvalue.h>

#include <functional>

namespace qbs {
namespace Internal {
class EvaluatorScriptClass;
class FileTags;
class Logger;
class PropertyDeclaration;
class ScriptEngine;

class QBS_AUTOTEST_EXPORT Evaluator : private ItemObserver
{
    friend class SVConverter;

public:
    Evaluator(ScriptEngine *scriptEngine);
    ~Evaluator() override;

    ScriptEngine *engine() const { return m_scriptEngine; }
    QScriptValue property(const Item *item, const QString &name);

    QScriptValue value(const Item *item, const QString &name, bool *propertySet = nullptr);
    bool boolValue(const Item *item, const QString &name, bool *propertyWasSet = nullptr);
    int intValue(const Item *item, const QString &name, int defaultValue = 0,
                 bool *propertyWasSet = nullptr);
    FileTags fileTagsValue(const Item *item, const QString &name, bool *propertySet = nullptr);
    QString stringValue(const Item *item, const QString &name,
                        const QString &defaultValue = QString(), bool *propertyWasSet = nullptr);
    QStringList stringListValue(const Item *item, const QString &name,
                                bool *propertyWasSet = nullptr);

    void convertToPropertyType(const PropertyDeclaration& decl, const CodeLocation &loc,
                               QScriptValue &v);

    QScriptValue scriptValue(const Item *item);

    struct FileContextScopes
    {
        QScriptValue fileScope;
        QScriptValue importScope;
    };

    FileContextScopes fileContextScopes(const FileContextConstPtr &file);

    void setCachingEnabled(bool enabled);

    PropertyDependencies propertyDependencies() const;
    void clearPropertyDependencies();

    void handleEvaluationError(const Item *item, const QString &name,
            const QScriptValue &scriptValue);

    void setPathPropertiesBaseDir(const QString &dirPath);
    void clearPathPropertiesBaseDir();

    bool isNonDefaultValue(const Item *item, const QString &name) const;
private:
    void onItemPropertyChanged(Item *item) override;
    bool evaluateProperty(QScriptValue *result, const Item *item, const QString &name,
            bool *propertyWasSet);

    ScriptEngine *m_scriptEngine;
    EvaluatorScriptClass *m_scriptClass;
    mutable QHash<const Item *, QScriptValue> m_scriptValueMap;
    mutable QHash<FileContextConstPtr, FileContextScopes> m_fileContextScopesMap;
};

void throwOnEvaluationError(ScriptEngine *engine, const QScriptValue &scriptValue,
                            const std::function<CodeLocation()> &provideFallbackCodeLocation);

class EvalCacheEnabler
{
public:
    EvalCacheEnabler(Evaluator *evaluator) : m_evaluator(evaluator)
    {
        m_evaluator->setCachingEnabled(true);
    }

    ~EvalCacheEnabler() { m_evaluator->setCachingEnabled(false); }

private:
    Evaluator * const m_evaluator;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_EVALUATOR_H
