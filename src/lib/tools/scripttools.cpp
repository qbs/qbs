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

#include "scripttools.h"
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValueIterator>
#include <QDebug>

QT_BEGIN_NAMESPACE

QDataStream &operator<< (QDataStream &s, const QScriptProgram &script)
{
    s << script.sourceCode()
      << script.fileName()
      << script.firstLineNumber();
    return s;
}

QDataStream &operator>> (QDataStream &s, QScriptProgram &script)
{
    QString fileName, sourceCode;
    int lineNumber;
    s >> sourceCode
      >> fileName
      >> lineNumber;
    script = QScriptProgram(sourceCode, fileName, lineNumber);
    return s;
}

QT_END_NAMESPACE

namespace qbs {

static const bool debugJSImports = false;

QScriptValue addJSImport(QScriptEngine *engine,
                         const QScriptProgram &program,
                         const QString &id)
{
    if (debugJSImports)
        qDebug() << "addJSImport: " << id;
    QScriptValue activationObject = engine->currentContext()->activationObject();
    QScriptValue targetObject = activationObject.property(id);
    QScriptValue result = addJSImport(engine, program, targetObject);
    activationObject.setProperty(id, targetObject);
    return result;
}

QScriptValue addJSImport(QScriptEngine *engine,
                         const QScriptProgram &program,
                         QScriptValue &targetObject)
{
    QSet<QString> globalPropertyNames;
    {
        QScriptValueIterator it(engine->globalObject());
        while (it.hasNext()) {
            it.next();
            globalPropertyNames += it.name();
        }
    }

    engine->pushContext();
    QScriptValue result = engine->evaluate(program);
    QScriptValue activationObject = engine->currentContext()->activationObject();
    engine->popContext();
    if (result.isError())
        return result;

    if (targetObject.isValid() && !targetObject.isUndefined()) {
        // try to merge imports with the same target into the same object
        // it is necessary for library imports that have multiple js files
        QScriptValueIterator it(activationObject);
        while (it.hasNext()) {
            it.next();
            if (debugJSImports)
                qDebug() << "mergeJSImport copying property" << it.name();
            targetObject.setProperty(it.name(), it.value());
        }
    } else {
        targetObject = activationObject;
    }

    // Copy new global properties to the target object and remove them from
    // the global object. This is to support direct variable assignments
    // without the 'var' keyword in JavaScript files.
    QScriptValueIterator it(engine->globalObject());
    while (it.hasNext()) {
        it.next();
        if (globalPropertyNames.contains(it.name()))
            continue;

        if (debugJSImports)
            qDebug() << "inserting global property" << it.name() << it.value().toString();

        targetObject.setProperty(it.name(), it.value());
        it.remove();
    }

    return result;
}

void setConfigProperty(QVariantMap &cfg, const QStringList &name, const QVariant &value)
{
    if (name.length() == 1) {
        cfg.insert(name.first(), value);
    } else {
        QVariant &subCfg = cfg[name.first()];
        QVariantMap subCfgMap = subCfg.toMap();
        setConfigProperty(subCfgMap, name.mid(1), value);
        subCfg = subCfgMap;
    }
}

QVariant getConfigProperty(const QVariantMap &cfg, const QStringList &name)
{
    if (name.length() == 1) {
        return cfg.value(name.first());
    } else {
        return getConfigProperty(cfg.value(name.first()).toMap(), name.mid(1));
    }
}

} // namespace qbs
