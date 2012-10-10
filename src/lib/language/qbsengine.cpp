/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#include "qbsengine.h"
#include <tools/error.h>
#include <tools/logger.h>
#include <QScriptProgram>
#include <QScriptValueIterator>

namespace qbs {

const bool debugJSImports = false;

QbsEngine::QbsEngine(QObject *parent)
    : QScriptEngine(parent)
{
}

QbsEngine::~QbsEngine()
{
}

void QbsEngine::import(const JsImports &jsImports, QScriptValue scope, QScriptValue targetObject)
{
    for (JsImports::const_iterator it = jsImports.begin(); it != jsImports.end(); ++it)
        import(*it, scope, targetObject);
}

void QbsEngine::import(const JsImport &jsImport, QScriptValue scope, QScriptValue targetObject)
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

void QbsEngine::clearImportsCache()
{
    m_jsImportCache.clear();
}

void QbsEngine::importProgram(const QScriptProgram &program, const QScriptValue &scope,
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

    if (targetObject.isValid() && !targetObject.isUndefined()) {
        // try to merge imports with the same target into the same object
        // it is necessary for library imports that have multiple js files
        QScriptValueIterator it(activationObject);
        while (it.hasNext()) {
            it.next();
            if (debugJSImports)
                qbsDebug() << "[ENGINE] Merge. Copying property " << it.name();
            targetObject.setProperty(it.name(), it.value());
        }
    } else {
        targetObject = activationObject;
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

} // namespace qbs
