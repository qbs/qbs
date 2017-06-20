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

#ifndef QBS_SCRIPTENGINE_H
#define QBS_SCRIPTENGINE_H

#include "forward_decls.h"
#include "property.h"
#include <logging/logger.h>
#include <tools/codelocation.h>
#include <tools/filetime.h>
#include <tools/set.h>

#include <QtCore/qdir.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qprocess.h>
#include <QtCore/qstring.h>

#include <QtScript/qscriptengine.h>

#include <stack>
#include <vector>

namespace qbs {
namespace Internal {
class Artifact;
class JsImport;
class ScriptImporter;
class ScriptPropertyObserver;

enum class EvalContext { PropertyEvaluation, ProbeExecution, RuleExecution, JsCommand };
class DubiousContext
{
public:
    enum Suggestion { NoSuggestion, SuggestMoving };
    DubiousContext(EvalContext c, Suggestion s = NoSuggestion) : context(c), suggestion(s) { }
    EvalContext context;
    Suggestion suggestion;
};
using DubiousContextList = std::vector<DubiousContext>;

class QBS_AUTOTEST_EXPORT ScriptEngine : public QScriptEngine
{
    Q_OBJECT
public:
    ScriptEngine(Logger &logger, EvalContext evalContext, QObject *parent = 0);
    ~ScriptEngine();

    Logger &logger() const { return m_logger; }
    void import(const FileContextBaseConstPtr &fileCtx, QScriptValue &targetObject);
    void import(const JsImport &jsImport, QScriptValue &targetObject);
    void clearImportsCache();

    void setEvalContext(EvalContext c) { m_evalContext = c; }
    EvalContext evalContext() const { return m_evalContext; }
    void checkContext(const QString &operation, const DubiousContextList &dubiousContexts);

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

    void enableProfiling(bool enable);

    void setPropertyCacheEnabled(bool enable) { m_propertyCacheEnabled = enable; }
    bool isPropertyCacheEnabled() const { return m_propertyCacheEnabled; }
    void addToPropertyCache(const QString &moduleName, const QString &propertyName,
                            const PropertyMapConstPtr &propertyMap, const QVariant &value);
    QVariant retrieveFromPropertyCache(const QString &moduleName, const QString &propertyName,
                                       const PropertyMapConstPtr &propertyMap);

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
    void addDirectoryEntriesResult(const QString &path, QDir::Filters filters,
                                   const QStringList &entries);
    void addFileLastModifiedResult(const QString &filePath, const FileTime &fileTime);
    QHash<QString, QString> canonicalFilePathResults() const { return m_canonicalFilePathResult; }
    QHash<QString, bool> fileExistsResults() const { return m_fileExistsResult; }
    QHash<std::pair<QString, quint32>, QStringList> directoryEntriesResults() const
    {
        return m_directoryEntriesResult;
    }

    QHash<QString, FileTime> fileLastModifiedResults() const { return m_fileLastModifiedResult; }
    Set<QString> imports() const;
    static QScriptValueList argumentList(const QStringList &argumentNames,
            const QScriptValue &context);
    void registerOwnedVariantMap(QVariantMap *vm) { m_ownedVariantMaps.append(vm); }

    QStringList uncaughtExceptionBacktraceOrEmpty() const {
        return hasUncaughtException() ? uncaughtExceptionBacktrace() : QStringList();
    }
    bool hasErrorOrException(const QScriptValue &v) const {
        return v.isError() || hasUncaughtException();
    }
    QScriptValue lastErrorValue(const QScriptValue &v) const {
        return v.isError() ? v : uncaughtException();
    }
    QString lastErrorString(const QScriptValue &v) const { return lastErrorValue(v).toString(); }
    CodeLocation lastErrorLocation(const QScriptValue &v,
                                   const CodeLocation &fallbackLocation = CodeLocation()) const;
    ErrorInfo lastError(const QScriptValue &v,
                        const CodeLocation &fallbackLocation = CodeLocation()) const;

    void cancel();

    // The active flag is different from QScriptEngine::isEvaluating.
    // It is set and cleared externally for example by the rule execution code.
    bool isActive() const { return m_active; }
    void setActive(bool on) { m_active = on; }

    using QScriptEngine::newFunction;

    template <typename T, typename E,
              typename = typename std::enable_if<std::is_pointer<T>::value>::type,
              typename = typename std::enable_if<std::is_pointer<E>::value>::type,
              typename = typename std::enable_if<std::is_base_of<
                QScriptEngine, typename std::remove_pointer<E>::type>::value>::type
              > QScriptValue newFunction(QScriptValue (*signature)(QScriptContext *, E, T), T arg) {
        return QScriptEngine::newFunction(
                    reinterpret_cast<FunctionWithArgSignature>(signature),
                    reinterpret_cast<void *>(const_cast<
                                             typename std::add_pointer<
                                             typename std::remove_const<
                                             typename std::remove_pointer<T>::type>::type>::type>(
                                                 arg)));
    }

    QScriptClass *modulePropertyScriptClass() const;
    void setModulePropertyScriptClass(QScriptClass *modulePropertyScriptClass);

private:
    QScriptValue newFunction(FunctionWithArgSignature signature, void *arg) Q_DECL_EQ_DELETE;

    void abort();

    void installQbsBuiltins();
    void extendJavaScriptBuiltins();
    void installFunction(const QString &name, int length, QScriptValue *functionValue,
                         FunctionSignature f, QScriptValue *targetObject);
    void installQbsFunction(const QString &name, int length, FunctionSignature f);
    void installConsoleFunction(const QString &name,
                                QScriptValue (*f)(QScriptContext *, QScriptEngine *, Logger *));
    void installImportFunctions();
    void uninstallImportFunctions();
    void importFile(const QString &filePath, QScriptValue &targetObject);
    static QScriptValue js_loadExtension(QScriptContext *context, QScriptEngine *qtengine);
    static QScriptValue js_loadFile(QScriptContext *context, QScriptEngine *qtengine);
    static QScriptValue js_require(QScriptContext *context, QScriptEngine *qtengine);

    class PropertyCacheKey
    {
    public:
        PropertyCacheKey(const QString &moduleName, const QString &propertyName,
                         const PropertyMapConstPtr &propertyMap);
    private:
        const QString m_moduleName;
        const QString m_propertyName;
        const PropertyMapConstPtr m_propertyMap;

        friend bool operator==(const PropertyCacheKey &lhs, const PropertyCacheKey &rhs);
        friend uint qHash(const ScriptEngine::PropertyCacheKey &k, uint seed);
    };

    friend bool operator==(const PropertyCacheKey &lhs, const PropertyCacheKey &rhs);
    friend uint qHash(const ScriptEngine::PropertyCacheKey &k, uint seed);

    ScriptImporter *m_scriptImporter;
    QScriptClass *m_modulePropertyScriptClass;
    QHash<JsImport, QScriptValue> m_jsImportCache;
    bool m_propertyCacheEnabled;
    bool m_active;
    QHash<PropertyCacheKey, QVariant> m_propertyCache;
    PropertySet m_propertiesRequestedInScript;
    QHash<QString, PropertySet> m_propertiesRequestedFromArtifact;
    Logger &m_logger;
    QScriptValue m_definePropertyFunction;
    QScriptValue m_emptyFunction;
    QProcessEnvironment m_environment;
    QHash<QString, QString> m_usedEnvironment;
    QHash<QString, QString> m_canonicalFilePathResult;
    QHash<QString, bool> m_fileExistsResult;
    QHash<std::pair<QString, quint32>, QStringList> m_directoryEntriesResult;
    QHash<QString, FileTime> m_fileLastModifiedResult;
    std::stack<QString> m_currentDirPathStack;
    std::stack<QStringList> m_extensionSearchPathsStack;
    QScriptValue m_loadFileFunction;
    QScriptValue m_loadExtensionFunction;
    QScriptValue m_requireFunction;
    QScriptValue m_qbsObject;
    QScriptValue m_consoleObject;
    QScriptValue m_cancelationError;
    QList<QVariantMap *> m_ownedVariantMaps;
    qint64 m_elapsedTimeImporting = -1;
    EvalContext m_evalContext;
};

class EvalContextSwitcher
{
public:
    EvalContextSwitcher(ScriptEngine *engine, EvalContext newContext)
        : m_engine(engine), m_oldContext(engine->evalContext())
    {
        engine->setEvalContext(newContext);
    }

    ~EvalContextSwitcher() { m_engine->setEvalContext(m_oldContext); }

private:
    ScriptEngine * const m_engine;
    const EvalContext m_oldContext;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_SCRIPTENGINE_H
