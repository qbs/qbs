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

#ifndef QBS_SCRIPTENGINE_H
#define QBS_SCRIPTENGINE_H

#include "jsimports.h"
#include "forward_decls.h"
#include <language/property.h>
#include <logging/logger.h>
#include <tools/qbs_export.h>

#include <QHash>
#include <QPair>
#include <QScriptEngine>

namespace qbs {
namespace Internal {

class ScriptPropertyObserver;

// FIXME: Exported for qbs-qmltypes
class QBS_EXPORT ScriptEngine : public QScriptEngine
{
    Q_OBJECT
public:
    ScriptEngine(const Logger &logger, QObject *parent = 0);
    ~ScriptEngine();

    void setLogger(const Logger &logger) { m_logger = logger; }
    void import(const JsImports &jsImports, QScriptValue scope, QScriptValue targetObject);
    void import(const JsImport &jsImport, QScriptValue scope, QScriptValue targetObject);
    void clearImportsCache();

    void addProperty(const Property &property) { m_properties += property; }
    void clearProperties() { m_properties.clear(); }
    PropertyList properties() const { return m_properties; }

    void addToPropertyCache(const QString &moduleName, const QString &propertyName,
        const PropertyMapConstPtr &propertyMap, QScriptValue value);
    QScriptValue retrieveFromPropertyCache(const QString &moduleName, const QString &propertyName,
            const PropertyMapConstPtr &propertyMap);

    void defineProperty(QScriptValue &object, const QString &name, const QScriptValue &descriptor);
    void setObservedProperty(QScriptValue &object, const QString &name, const QScriptValue &value,
                             ScriptPropertyObserver *observer);

private:
    void importProgram(const QScriptProgram &program, const QScriptValue &scope,
                       QScriptValue &targetObject);

    QHash<QString, QScriptValue> m_jsImportCache;
    QHash<QPair<QString, PropertyMapConstPtr>, QScriptValue> m_propertyCache;
    PropertyList m_properties;
    Logger m_logger;
    QScriptValue m_definePropertyFunction;
    QScriptValue m_emptyFunction;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_SCRIPTENGINE_H
