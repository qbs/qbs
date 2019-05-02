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
#include <buildgraph/requestedartifacts.h>
#include <buildgraph/requesteddependencies.h>
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

#include <memory>
#include <mutex>
#include <stack>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace qbs {
namespace Internal {
class Artifact;
class JsImport;
class PrepareScriptObserver;
class ScriptImporter;
class ScriptPropertyObserver;

enum class EvalContext {
    PropertyEvaluation, ProbeExecution, ModuleProvider, RuleExecution, JsCommand
};
class DubiousContext
{
public:
    enum Suggestion { NoSuggestion, SuggestMoving };
    DubiousContext(EvalContext c, Suggestion s = NoSuggestion) : context(c), suggestion(s) { }
    EvalContext context;
    Suggestion suggestion;
};
using DubiousContextList = std::vector<DubiousContext>;


/*
 * ScriptObject that acquires resources, for example a file handle.
 * The ScriptObject should have QtOwnership and deleteLater() itself in releaseResources.
 */
class ResourceAcquiringScriptObject
{
public:
    virtual ~ResourceAcquiringScriptObject() = default;
    virtual void releaseResources() = 0;
};

enum class ObserveMode { Enabled, Disabled };

class QBS_AUTOTEST_EXPORT ScriptEngine : public QScriptEngine
{
    Q_OBJECT
    ScriptEngine(Logger &logger, EvalContext evalContext, QObject *parent = nullptr);
public:
    static ScriptEngine *create(Logger &logger, EvalContext evalContext, QObject *parent = nullptr);
    ~ScriptEngine() override;

    Logger &logger() const { return m_logger; }
    void import(const FileContextBaseConstPtr &fileCtx, QScriptValue &targetObject,
                ObserveMode observeMode);
    void clearImportsCache();

    void setEvalContext(EvalContext c) { m_evalContext = c; }
    EvalContext evalContext() const { return m_evalContext; }
    void checkContext(const QString &operation, const DubiousContextList &dubiousContexts);

    void addPropertyRequestedInScript(const Property &property) {
        m_propertiesRequestedInScript += property;
    }
    void addDependenciesArrayRequested(const ResolvedProduct *p)
    {
        m_productsWithRequestedDependencies.insert(p);
    }
    void setArtifactsMapRequested(const ResolvedProduct *product, bool forceUpdate)
    {
        m_requestedArtifacts.setAllArtifactTags(product, forceUpdate);
    }
    void setArtifactSetRequestedForTag(const ResolvedProduct *product, const FileTag &tag)
    {
        m_requestedArtifacts.setArtifactsForTag(product, tag);
    }
    void setNonExistingArtifactSetRequested(const ResolvedProduct *product, const QString &tag)
    {
        m_requestedArtifacts.setNonExistingTagRequested(product, tag);
    }
    void setArtifactsEnumerated(const ResolvedProduct *product)
    {
        m_requestedArtifacts.setArtifactsEnumerated(product);
    }
    void addPropertyRequestedFromArtifact(const Artifact *artifact, const Property &property);
    void addRequestedExport(const ResolvedProduct *product) { m_requestedExports.insert(product); }
    void clearRequestedProperties() {
        m_propertiesRequestedInScript.clear();
        m_propertiesRequestedFromArtifact.clear();
        m_importsRequestedInScript.clear();
        m_productsWithRequestedDependencies.clear();
        m_requestedArtifacts.clear();
        m_requestedExports.clear();
    }
    PropertySet propertiesRequestedInScript() const { return m_propertiesRequestedInScript; }
    QHash<QString, PropertySet> propertiesRequestedFromArtifact() const {
        return m_propertiesRequestedFromArtifact;
    }
    Set<const ResolvedProduct *> productsWithRequestedDependencies() const
    {
        return m_productsWithRequestedDependencies;
    }
    RequestedDependencies requestedDependencies() const
    {
        return RequestedDependencies(m_productsWithRequestedDependencies);
    }
    RequestedArtifacts requestedArtifacts() const { return m_requestedArtifacts; }
    Set<const ResolvedProduct *> requestedExports() const { return m_requestedExports; }

    void addImportRequestedInScript(qint64 importValueId);
    std::vector<QString> importedFilesUsedInScript() const;

    void setUsesIo() { m_usesIo = true; }
    void clearUsesIo() { m_usesIo = false; }
    bool usesIo() const { return m_usesIo; }

    void enableProfiling(bool enable);

    void setPropertyCacheEnabled(bool enable) { m_propertyCacheEnabled = enable; }
    bool isPropertyCacheEnabled() const { return m_propertyCacheEnabled; }
    void addToPropertyCache(const QString &moduleName, const QString &propertyName,
                            const PropertyMapConstPtr &propertyMap, const QVariant &value);
    QVariant retrieveFromPropertyCache(const QString &moduleName, const QString &propertyName,
                                       const PropertyMapConstPtr &propertyMap);

    void defineProperty(QScriptValue &object, const QString &name, const QScriptValue &descriptor);
    void setObservedProperty(QScriptValue &object, const QString &name, const QScriptValue &value);
    void unobserveProperties();
    void setDeprecatedProperty(QScriptValue &object, const QString &name, const QString &newName,
            const QScriptValue &value);
    PrepareScriptObserver *observer() const { return m_observer.get(); }

    QProcessEnvironment environment() const;
    void setEnvironment(const QProcessEnvironment &env);
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
              typename = std::enable_if_t<std::is_pointer<T>::value>,
              typename = std::enable_if_t<std::is_pointer<E>::value>,
              typename = std::enable_if_t<std::is_base_of<
                QScriptEngine, std::remove_pointer_t<E>>::value>
              > QScriptValue newFunction(QScriptValue (*signature)(QScriptContext *, E, T), T arg) {
        return QScriptEngine::newFunction(
                    reinterpret_cast<FunctionWithArgSignature>(signature),
                    reinterpret_cast<void *>(const_cast<
                                             std::add_pointer_t<
                                             std::remove_const_t<
                                             std::remove_pointer_t<T>>>>(arg)));
    }

    QScriptClass *modulePropertyScriptClass() const;
    void setModulePropertyScriptClass(QScriptClass *modulePropertyScriptClass);

    QScriptClass *productPropertyScriptClass() const { return m_productPropertyScriptClass; }
    void setProductPropertyScriptClass(QScriptClass *productPropertyScriptClass)
    {
        m_productPropertyScriptClass = productPropertyScriptClass;
    }

    QScriptClass *artifactsScriptClass() const { return m_artifactsScriptClass; }
    void setArtifactsScriptClass(QScriptClass *artifactsScriptClass)
    {
        m_artifactsScriptClass = artifactsScriptClass;
    }

    void addResourceAcquiringScriptObject(ResourceAcquiringScriptObject *obj);
    void releaseResourcesOfScriptObjects();

    QScriptValue &productScriptValuePrototype(const ResolvedProduct *product)
    {
        return m_productScriptValues[product];
    }

    QScriptValue &projectScriptValue(const ResolvedProject *project)
    {
        return m_projectScriptValues[project];
    }

    QScriptValue &moduleScriptValuePrototype(const ResolvedModule *module)
    {
        return m_moduleScriptValues[module];
    }

private:
    QScriptValue newFunction(FunctionWithArgSignature signature, void *arg) Q_DECL_EQ_DELETE;

    void abort();

    bool gatherFileResults() const;

    void installQbsBuiltins();
    void extendJavaScriptBuiltins();
    void installFunction(const QString &name, int length, QScriptValue *functionValue,
                         FunctionSignature f, QScriptValue *targetObject);
    void installQbsFunction(const QString &name, int length, FunctionSignature f);
    void installConsoleFunction(const QString &name,
                                QScriptValue (*f)(QScriptContext *, QScriptEngine *, Logger *));
    void installImportFunctions();
    void uninstallImportFunctions();
    void import(const JsImport &jsImport, QScriptValue &targetObject);
    void observeImport(QScriptValue &jsImport);
    void importFile(const QString &filePath, QScriptValue &targetObject);
    static QScriptValue js_loadExtension(QScriptContext *context, QScriptEngine *qtengine);
    static QScriptValue js_loadFile(QScriptContext *context, QScriptEngine *qtengine);
    static QScriptValue js_require(QScriptContext *context, QScriptEngine *qtengine);

    class PropertyCacheKey
    {
    public:
        PropertyCacheKey(QString moduleName, QString propertyName,
                         PropertyMapConstPtr propertyMap);
    private:
        const QString m_moduleName;
        const QString m_propertyName;
        const PropertyMapConstPtr m_propertyMap;

        friend bool operator==(const PropertyCacheKey &lhs, const PropertyCacheKey &rhs);
        friend uint qHash(const ScriptEngine::PropertyCacheKey &k, uint seed);
    };

    friend bool operator==(const PropertyCacheKey &lhs, const PropertyCacheKey &rhs);
    friend uint qHash(const ScriptEngine::PropertyCacheKey &k, uint seed);

    static std::mutex m_creationDestructionMutex;
    ScriptImporter *m_scriptImporter;
    QScriptClass *m_modulePropertyScriptClass;
    QScriptClass *m_productPropertyScriptClass = nullptr;
    QScriptClass *m_artifactsScriptClass = nullptr;
    QHash<JsImport, QScriptValue> m_jsImportCache;
    std::unordered_map<QString, QScriptValue> m_jsFileCache;
    bool m_propertyCacheEnabled;
    bool m_active;
    QHash<PropertyCacheKey, QVariant> m_propertyCache;
    PropertySet m_propertiesRequestedInScript;
    QHash<QString, PropertySet> m_propertiesRequestedFromArtifact;
    Logger &m_logger;
    QScriptValue m_definePropertyFunction;
    QScriptValue m_emptyFunction;
    QProcessEnvironment m_environment;
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
    qint64 m_elapsedTimeImporting = -1;
    bool m_usesIo = false;
    EvalContext m_evalContext;
    std::vector<ResourceAcquiringScriptObject *> m_resourceAcquiringScriptObjects;
    const std::unique_ptr<PrepareScriptObserver> m_observer;
    std::vector<std::tuple<QScriptValue, QString, QScriptValue>> m_observedProperties;
    std::vector<QScriptValue> m_requireResults;
    std::unordered_map<qint64, std::vector<QString>> m_filePathsPerImport;
    std::vector<qint64> m_importsRequestedInScript;
    Set<const ResolvedProduct *> m_productsWithRequestedDependencies;
    RequestedArtifacts m_requestedArtifacts;
    Set<const ResolvedProduct *> m_requestedExports;
    ObserveMode m_observeMode = ObserveMode::Disabled;
    std::unordered_map<const ResolvedProduct *, QScriptValue> m_productScriptValues;
    std::unordered_map<const ResolvedProject *, QScriptValue> m_projectScriptValues;
    std::unordered_map<const ResolvedModule *, QScriptValue> m_moduleScriptValues;
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
