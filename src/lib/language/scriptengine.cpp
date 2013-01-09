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

#include "scriptengine.h"

#include <logging/logger.h>
#include <tools/error.h>

#include <QFile>
#include <QScriptProgram>
#include <QScriptValueIterator>
#include <QSet>
#include <QTextStream>

namespace qbs {
namespace Internal {

const bool debugJSImports = false;

ScriptEngine::ScriptEngine(QObject *parent)
    : QScriptEngine(parent)
{
}

ScriptEngine::~ScriptEngine()
{
}

void ScriptEngine::import(const JsImports &jsImports, QScriptValue scope, QScriptValue targetObject)
{
    for (JsImports::const_iterator it = jsImports.begin(); it != jsImports.end(); ++it)
        import(*it, scope, targetObject);
}

void ScriptEngine::import(const JsImport &jsImport, QScriptValue scope, QScriptValue targetObject)
{
    Q_ASSERT(!scope.isValid() || scope.isObject());
    Q_ASSERT(targetObject.isObject());
    Q_ASSERT(targetObject.engine() == this);

    if (debugJSImports)
        qbsDebug() << "[ENGINE] import into " << jsImport.scopeName;

    foreach (const QString &fileName, jsImport.fileNames) {
        QScriptValue jsImportValue;
        jsImportValue = m_jsImportCache.value(fileName);
        if (jsImportValue.isValid()) {
            if (debugJSImports)
                qbsDebug() << "[ENGINE] " << fileName << " (cache hit)";
            targetObject.setProperty(jsImport.scopeName, jsImportValue);
        } else {
            if (debugJSImports)
                qbsDebug() << "[ENGINE] " << fileName << " (cache miss)";
            QFile file(fileName);
            if (!file.open(QFile::ReadOnly))
                throw Error(tr("Cannot open '%1'.").arg(fileName));
            const QString sourceCode = QTextStream(&file).readAll();
            file.close();
            QScriptProgram program(sourceCode, fileName);
            importProgram(program, scope, jsImportValue);
            targetObject.setProperty(jsImport.scopeName, jsImportValue);
            m_jsImportCache.insert(fileName, jsImportValue);
        }
    }
}

void ScriptEngine::clearImportsCache()
{
    m_jsImportCache.clear();
}

void ScriptEngine::importProgram(const QScriptProgram &program, const QScriptValue &scope,
                               QScriptValue &targetObject)
{
    QSet<QString> globalPropertyNames;
    {
        QScriptValueIterator it(globalObject());
        while (it.hasNext()) {
            it.next();
            globalPropertyNames += it.name();
        }
    }

    pushContext();
    if (scope.isObject())
        currentContext()->pushScope(scope);
    QScriptValue result = evaluate(program);
    QScriptValue activationObject = currentContext()->activationObject();
    if (scope.isObject())
        currentContext()->popScope();
    popContext();
    if (result.isError())
        throw Error(tr("Error when importing '%1': %2").arg(program.fileName(), result.toString()));

    // If targetObject is already an object, it doesn't get overwritten but enhanced by the
    // contents of the .js file.
    // This is necessary for library imports that consist of multiple js files.
    if (!targetObject.isObject())
        targetObject = newObject();

    // Copy every property of the activation object to the target object.
    // We do not just save a reference to the activation object, because QScriptEngine contains
    // special magic for activation objects that leads to unanticipated results.
    {
        QScriptValueIterator it(activationObject);
        while (it.hasNext()) {
            it.next();
            if (debugJSImports)
                qbsDebug() << "[ENGINE] Copying property " << it.name();
            targetObject.setProperty(it.name(), it.value());
        }
    }

    // Copy new global properties to the target object and remove them from
    // the global object. This is to support direct variable assignments
    // without the 'var' keyword in JavaScript files.
    QScriptValueIterator it(globalObject());
    while (it.hasNext()) {
        it.next();
        if (globalPropertyNames.contains(it.name()))
            continue;

        if (debugJSImports)
            qbsDebug() << "[ENGINE] inserting global property "
                       << it.name() << " " << it.value().toString();

        targetObject.setProperty(it.name(), it.value());
        it.remove();
    }
}

} // namespace Internal
} // namespace qbs
