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

#ifndef QBS_SCRIPTENGINE_H
#define QBS_SCRIPTENGINE_H

#include "forward_decls.h"
#include "property.h"
#include <logging/logger.h>
#include <tools/filetime.h>

#include <QHash>
#include <QList>
#include <QProcessEnvironment>
#include <QScriptEngine>
#include <QStack>
#include <QString>

namespace qbs {
namespace Internal {
class Artifact;
class JsImport;
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
    PropertySet propertiesRequestedInScript() const { return m_propertiesRequestedInScript; }
    QHash<QString, PropertySet> propertiesRequestedFromArtifact() const {
        return m_propertiesRequestedFromArtifact;
    }

    void setPropertyCacheEnabled(bool enable) { m_propertyCacheEnabled = enable; }
    bool isPropertyCacheEnabled() const { return m_propertyCacheEnabled; }
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
    void addCanonicalFilePathResult(const QString &filePath, const QString &resultFilePath);
    void addFileExistsResult(const QString &filePath, bool exists);
    void addFileLastModifiedResult(const QString &filePath, const FileTime &fileTime);
    QHash<QString, QString> canonicalFilePathResults() const { return m_canonicalFilePathResult; }
    QHash<QString, bool> fileExistsResults() const { return m_fileExistsResult; }
    QHash<QString, FileTime> fileLastModifiedResults() const { return m_fileLastModifiedResult; }
    QSet<QString> imports() const;
    static QScriptValueList argumentList(const QStringList &argumentNames,
            const QScriptValue &context);
    void registerOwnedVariantMap(QVariantMap *vm) { m_ownedVariantMaps.append(vm); }


    bool hasErrorOrException(const QScriptValue &v) const {
        return v.isError() || hasUncaughtException();
    }
    QScriptValue lastErrorValue(const QScriptValue &v) const {
        return v.isError() ? v : uncaughtException();
    }
    QString lastErrorString(const QScriptValue &v) const { return lastErrorValue(v).toString(); }

    void cancel();

private:
    Q_INVOKABLE void abort();

    void installQbsBuiltins();
    void extendJavaScriptBuiltins();
    void installFunction(const QString &name, QScriptValue *functionValue, FunctionSignature f, QScriptValue *targetObject);
    void installQbsFunction(const QString &name, FunctionSignature f);
    void installImportFunctions();
    void uninstallImportFunctions();
    QScriptValue importFile(const QString &filePath, const QScriptValue &scope,
                            QScriptValue *targetObject = nullptr);
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

    QHash<JsImport, QScriptValue> m_jsImportCache;
    bool m_propertyCacheEnabled;
    QHash<PropertyCacheKey, QVariant> m_propertyCache;
    PropertySet m_propertiesRequestedInScript;
    QHash<QString, PropertySet> m_propertiesRequestedFromArtifact;
    Logger m_logger;
    QScriptValue m_definePropertyFunction;
    QScriptValue m_emptyFunction;
    QProcessEnvironment m_environment;
    QHash<QString, QString> m_usedEnvironment;
    QHash<QString, QString> m_canonicalFilePathResult;
    QHash<QString, bool> m_fileExistsResult;
    QHash<QString, FileTime> m_fileLastModifiedResult;
    QStack<QString> m_currentDirPathStack;
    QStack<QStringList> m_extensionSearchPathsStack;
    QScriptValue m_loadFileFunction;
    QScriptValue m_loadExtensionFunction;
    QScriptValue m_qbsObject;
    QScriptValue m_cancelationError;
    QList<QVariantMap *> m_ownedVariantMaps;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_SCRIPTENGINE_H
