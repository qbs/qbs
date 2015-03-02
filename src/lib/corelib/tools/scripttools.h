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

#ifndef QBS_SCRIPTTOOLS_H
#define QBS_SCRIPTTOOLS_H

#include <tools/qbs_export.h>

#include <QScriptEngine>
#include <QScriptProgram>
#include <QScriptValue>
#include <QStringList>
#include <QVariantMap>

QT_BEGIN_NAMESPACE

QDataStream &operator<< (QDataStream &s, const QScriptProgram &script);
QDataStream &operator>> (QDataStream &s, QScriptProgram &script);

QT_END_NAMESPACE

namespace qbs {

QBS_EXPORT QString toJSLiteral(const bool b);
QBS_EXPORT QString toJSLiteral(const QString &str);
QBS_EXPORT QString toJSLiteral(const QStringList &strs);
QBS_EXPORT QString toJSLiteral(const QVariant &val);

namespace Internal {

template <typename C>
QScriptValue toScriptValue(QScriptEngine *scriptEngine, const C &container)
{
    QScriptValue v = scriptEngine->newArray(container.count());
    int i = 0;
    foreach (const typename C::value_type &item, container)
        v.setProperty(i++, scriptEngine->toScriptValue(item));
    return v;
}

void setConfigProperty(QVariantMap &cfg, const QStringList &name, const QVariant &value);
QVariant getConfigProperty(const QVariantMap &cfg, const QStringList &name);

/**
 * @brief push/pop a scope on a QScriptContext the RAII way.
 */
class ScriptContextScopePusher
{
public:
    ScriptContextScopePusher(QScriptContext *scriptContext, const QScriptValue &value)
        : m_scriptContext(scriptContext)
    {
        m_scriptContext->pushScope(value);
    }

    ~ScriptContextScopePusher()
    {
        m_scriptContext->popScope();
    }

private:
    QScriptContext *m_scriptContext;
};

template <class T>
void attachPointerTo(QScriptValue &scriptValue, T *ptr)
{
    QVariant v;
    v.setValue<quintptr>(reinterpret_cast<quintptr>(ptr));
    scriptValue.setData(scriptValue.engine()->newVariant(v));
}

template <class T>
T *attachedPointer(const QScriptValue &scriptValue)
{
    const quintptr ptr = scriptValue.data().toVariant().value<quintptr>();
    return reinterpret_cast<T *>(ptr);
}

} // namespace Internal
} // namespace qbs

#endif // QBS_SCRIPTTOOLS_H
