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

#ifndef QBS_EVALUATOR_H
#define QBS_EVALUATOR_H

#include "forward_decls.h"
#include "itemobserver.h"

#include <QHash>
#include <QScriptValue>

namespace qbs {
namespace Internal {
class EvaluatorScriptClass;
class FileTags;
class Logger;
class ScriptEngine;

class Evaluator : private ItemObserver
{
    friend class SVConverter;

public:
    Evaluator(ScriptEngine *scriptEngine, const Logger &logger);
    virtual ~Evaluator();

    ScriptEngine *engine() const { return m_scriptEngine; }
    QScriptValue property(const Item *item, const QString &name);

    QScriptValue value(const Item *item, const QString &name, bool *propertySet = 0);
    bool boolValue(const Item *item, const QString &name, bool defaultValue = false,
                   bool *propertyWasSet = 0);
    FileTags fileTagsValue(const Item *item, const QString &name, bool *propertySet = 0);
    QString stringValue(const Item *item, const QString &name,
                        const QString &defaultValue = QString(), bool *propertyWasSet = 0);
    QStringList stringListValue(const Item *item, const QString &name, bool *propertyWasSet = 0);

    QScriptValue scriptValue(const Item *item);
    QScriptValue fileScope(const FileContextConstPtr &file);

    void setCachingEnabled(bool enabled);

    void handleEvaluationError(const Item *item, const QString &name,
            const QScriptValue &scriptValue);
private:
    void onItemPropertyChanged(Item *item);
    void onItemDestroyed(Item *item);
    bool evaluateProperty(QScriptValue *result, const Item *item, const QString &name,
            bool *propertyWasSet);

    ScriptEngine *m_scriptEngine;
    EvaluatorScriptClass *m_scriptClass;
    mutable QHash<const Item *, QScriptValue> m_scriptValueMap;
    mutable QHash<FileContextConstPtr, QScriptValue> m_fileScopeMap;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_EVALUATOR_H
