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

#include <quickjs.h>

#include <QtCore/qhash.h>

#include <functional>
#include <mutex>
#include <optional>
#include <stack>

namespace qbs {
namespace Internal {
class EvaluationData;
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
    JSClassID classId() const { return m_scriptClass; }
    JSValue property(const Item *item, const QString &name);

    JSValue value(const Item *item, const QString &name, bool *propertySet = nullptr);
    bool boolValue(const Item *item, const QString &name, bool *propertyWasSet = nullptr);
    int intValue(const Item *item, const QString &name, int defaultValue = 0,
                 bool *propertyWasSet = nullptr);
    FileTags fileTagsValue(const Item *item, const QString &name, bool *propertySet = nullptr);
    QString stringValue(const Item *item, const QString &name,
                        const QString &defaultValue = QString(), bool *propertyWasSet = nullptr);
    QStringList stringListValue(const Item *item, const QString &name,
                                bool *propertyWasSet = nullptr);
    std::optional<QStringList> optionalStringListValue(const Item *item, const QString &name,
                                bool *propertyWasSet = nullptr);

    QVariant variantValue(const Item *item, const QString &name, bool *propertySet = nullptr);

    void convertToPropertyType(const PropertyDeclaration& decl, const CodeLocation &loc,
                               JSValue &v);

    JSValue scriptValue(const Item *item);

    struct FileContextScopes
    {
        JSValue fileScope = JS_UNDEFINED;
        JSValue importScope = JS_UNDEFINED;
    };

    FileContextScopes fileContextScopes(const FileContextConstPtr &file);

    void setCachingEnabled(bool enabled) { m_valueCacheEnabled = enabled; }
    bool cachingEnabled() const { return m_valueCacheEnabled; }
    void clearCache(const Item *item);
    void invalidateCache(const Item *item);
    void clearCacheIfInvalidated(EvaluationData &edata);

    PropertyDependencies &propertyDependencies() { return m_propertyDependencies; }
    void clearPropertyDependencies() { m_propertyDependencies.clear(); }

    std::stack<QualifiedId> &requestedProperties() { return m_requestedProperties; }

    void handleEvaluationError(const Item *item, const QString &name);

    QString pathPropertiesBaseDir() const { return m_pathPropertiesBaseDir; }
    void setPathPropertiesBaseDir(const QString &dirPath) { m_pathPropertiesBaseDir = dirPath; }
    void clearPathPropertiesBaseDir() { m_pathPropertiesBaseDir.clear(); }

    bool isNonDefaultValue(const Item *item, const QString &name) const;
private:
    void onItemPropertyChanged(Item *item) override { invalidateCache(item); }
    bool evaluateProperty(JSValue *result, const Item *item, const QString &name,
            bool *propertyWasSet);
    void clearCache(EvaluationData &edata);

    ScriptEngine * const m_scriptEngine;
    const JSClassID m_scriptClass;
    mutable QHash<const Item *, JSValue> m_scriptValueMap;
    mutable QHash<FileContextConstPtr, FileContextScopes> m_fileContextScopesMap;
    QString m_pathPropertiesBaseDir;
    PropertyDependencies m_propertyDependencies;
    std::stack<QualifiedId> m_requestedProperties;
    std::mutex m_cacheInvalidationMutex;
    Set<const Item *> m_invalidatedCaches;
    bool m_valueCacheEnabled = false;
};

void throwOnEvaluationError(ScriptEngine *engine,
                            const std::function<CodeLocation()> &provideFallbackCodeLocation);

class EvalCacheEnabler
{
public:
    EvalCacheEnabler(Evaluator *evaluator, const QString &baseDir) : m_evaluator(evaluator)
    {
        m_evaluator->setCachingEnabled(true);
        m_evaluator->setPathPropertiesBaseDir(baseDir);
    }

    ~EvalCacheEnabler() { reset(); }

    void reset()
    {
        m_evaluator->setCachingEnabled(false);
        m_evaluator->clearPathPropertiesBaseDir();
    }

private:
    Evaluator * const m_evaluator;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_EVALUATOR_H
