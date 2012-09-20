/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#ifndef SCRIPTTOOLS_H
#define SCRIPTTOOLS_H

#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include <QtScript/QScriptProgram>
#include <QtScript/QScriptValue>
#include <QtScript/QScriptEngine>

QT_BEGIN_NAMESPACE

QDataStream &operator<< (QDataStream &s, const QScriptProgram &script);
QDataStream &operator>> (QDataStream &s, QScriptProgram &script);

QT_END_NAMESPACE


namespace qbs {

template <typename C>
QScriptValue toScriptValue(QScriptEngine *scriptEngine, const C &container)
{
    QScriptValue v = scriptEngine->newArray(container.count());
    int i = 0;
    foreach (const typename C::value_type &item, container)
        v.setProperty(i++, scriptEngine->toScriptValue(item));
    return v;
}

QScriptValue addJSImport(QScriptEngine *engine,
                         const QScriptProgram &program,
                         const QString &id);
QScriptValue addJSImport(QScriptEngine *engine,
                         const QScriptProgram &program,
                         QScriptValue &targetObject);

void setConfigProperty(QVariantMap &cfg, const QStringList &name, const QVariant &value);
QVariant getConfigProperty(const QVariantMap &cfg, const QStringList &name);

/**
 * @brief push/pop a QScriptEngine's context the RAII way.
 */
class ScriptEngineContextPusher
{
public:
    ScriptEngineContextPusher(QScriptEngine *scriptEngine)
        : m_scriptEngine(scriptEngine)
    {
        m_scriptEngine->pushContext();
    }

    ~ScriptEngineContextPusher()
    {
        m_scriptEngine->popContext();
    }

private:
    QScriptEngine *m_scriptEngine;
};

} // namespace qbs

#endif // SCRIPTTOOLS_H
