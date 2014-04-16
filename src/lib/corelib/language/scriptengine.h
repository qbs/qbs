/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "forward_decls.h"
#include "jsimports.h"
#include "property.h"
#include <logging/logger.h>
#include <tools/filetime.h>

#include <QHash>
#include <QProcessEnvironment>
#include <QScriptEngine>
#include <QStack>
#include <QString>

namespace qbs {
namespace Internal {
class Artifact;

class ScriptPropertyObserver;

class ScriptEngine : public QScriptEngine
{
    Q_OBJECT
public:
    ScriptEngine(const Logger &logger, QObject *parent = 0);
    ~ScriptEngine();

    void setLogger(const Logger &logger) { m_logger = logger; }
    const Logger &logger() const { return m_logger; }
    void import(const FileContextBaseConstPtr &fileCtx, QScriptValue scope,
            QScriptValue targetObject);
    void import(const JsImport &jsImport, QScriptValue scope, QScriptValue targetObject);
    void clearImportsCache();

    void addPropertyRequestedInScript(const Property &property) {
        m_propertiesRequestedInScript += property;
    }
    void addPropertyRequestedFromArtifact(const Artifact *artifact, const Property &property);
    void clearRequestedProperties() {
        m_propertiesRequestedInScript.clear();
        m_propertiesRequestedFromArtifact.clear();
    }
    PropertyList propertiesRequestedInScript() const { return m_propertiesRequestedInScript; }
    QHash<QString, PropertyList> propertiesRequestedFromArtifact() const {
        return m_propertiesRequestedFromArtifact;
    }

    void addToPropertyCache(const QString &moduleName, const QString &propertyName,
            bool oneValue, const PropertyMapConstPtr &propertyMap, const QVariant &value);
    QVariant retrieveFromPropertyCache(const QString &moduleName, const QString &propertyName,
            bool oneValue, const PropertyMapConstPtr &propertyMap);

    void defineProperty(QScriptValue &object, const QString &name, const QScriptValue &descriptor);
    void setObservedProperty(QScriptValue &object, const QString &name, const QScriptValue &value,
                             ScriptPropertyObserver *observer);
    void setDeprecatedProperty(QScriptValue &object, const QString &name, const QString &newName,
            const QScriptValue &value);

    QProcessEnvironment environment() const;
    void setEnvironment(const QProcessEnvironment &env);
    void addEnvironmentVariable(const QString &name, const QString &value);
    QHash<QString, QString> usedEnvironment() const { return m_usedEnvironment; }
    void addFileExistsResult(const QString &filePath, bool exists);
    void addFileLastModifiedResult(const QString &filePath, FileTime fileTime);
    QHash<QString, bool> fileExistsResults() const { return m_fileExistsResult; }
    QHash<QString, FileTime> fileLastModifiedResults() const { return m_fileLastModifiedResult; }
    QSet<QString> imports() const;
    static QScriptValueList argumentList(const QStringList &argumentNames,
            const QScriptValue &context);

    class ScriptValueCache
    {
    public:
        ScriptValueCache() : observer(0), project(0), product(0) {}
        const void *observer;
        const void *project;
        const void *product;
        QScriptValue observerScriptValue;
        QScriptValue projectScriptValue;
        QScriptValue productScriptValue;
    };

    ScriptValueCache *scriptValueCache() { return &m_scriptValueCache; }

    bool hasErrorOrException(const QScriptValue &v) const {
        return v.isError() || hasUncaughtException();
    }

    void cancel();

private:
    Q_INVOKABLE void abort();

    void extendJavaScriptBuiltins();
    void installFunction(const QString &name, QScriptValue *functionValue, FunctionSignature f);
    void installImportFunctions();
    void uninstallImportFunctions();
    QScriptValue importFile(const QString &filePath, const QScriptValue &scope);
    void importProgram(const QScriptProgram &program, const QScriptValue &scope,
                       QScriptValue &targetObject);
    static QScriptValue js_loadExtension(QScriptContext *context, QScriptEngine *qtengine);
    static QScriptValue js_loadFile(QScriptContext *context, QScriptEngine *qtengine);

    class PropertyCacheKey
    {
    public:
        PropertyCacheKey(const QString &moduleName, const QString &propertyName, bool oneValue,
                         const PropertyMapConstPtr &propertyMap);
    private:
        const QString &m_moduleName;
        const QString &m_propertyName;
        const bool m_oneValue;
        const PropertyMapConstPtr &m_propertyMap;

        friend bool operator==(const PropertyCacheKey &lhs, const PropertyCacheKey &rhs);
        friend uint qHash(const ScriptEngine::PropertyCacheKey &k, uint seed);
    };

    friend bool operator==(const PropertyCacheKey &lhs, const PropertyCacheKey &rhs);
    friend uint qHash(const ScriptEngine::PropertyCacheKey &k, uint seed);

    ScriptValueCache m_scriptValueCache;
    QHash<QString, QScriptValue> m_jsImportCache;
    QHash<PropertyCacheKey, QVariant> m_propertyCache;
    PropertyList m_propertiesRequestedInScript;
    QHash<QString, PropertyList> m_propertiesRequestedFromArtifact;
    Logger m_logger;
    QScriptValue m_definePropertyFunction;
    QScriptValue m_emptyFunction;
    QProcessEnvironment m_environment;
    QHash<QString, QString> m_usedEnvironment;
    QHash<QString, bool> m_fileExistsResult;
    QHash<QString, FileTime> m_fileLastModifiedResult;
    QStack<QString> m_currentDirPathStack;
    QStack<QStringList> m_extensionSearchPathsStack;
    QScriptValue m_loadFileFunction;
    QScriptValue m_loadExtensionFunction;
    QScriptValue m_cancelationError;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_SCRIPTENGINE_H
