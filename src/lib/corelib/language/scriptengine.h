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
#include <quickjs.h>
#include <tools/codelocation.h>
#include <tools/filetime.h>
#include <tools/porting.h>
#include <tools/scripttools.h>
#include <tools/set.h>

#include <QtCore/qdir.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qprocess.h>
#include <QtCore/qstring.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <stack>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace qbs {
namespace Internal {
class Artifact;
class Evaluator;
class JsImport;
class PrepareScriptObserver;
class RuleNode;
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

enum class JsValueOwner { Caller, ScriptEngine }; // TODO: This smells like cheating. Should always be Caller.

enum class ObserveMode { Enabled, Disabled };

class QBS_AUTOTEST_EXPORT ScriptEngine
{
    struct PrivateTag {};
public:
    ScriptEngine(Logger &logger, EvalContext evalContext, PrivateTag);
    ~ScriptEngine();

    static std::unique_ptr<ScriptEngine> create(Logger &logger, EvalContext evalContext);
    static ScriptEngine *engineForRuntime(const JSRuntime *runtime);
    static ScriptEngine *engineForContext(const JSContext *ctx);
    static LookupResult doExtraScopeLookup(JSContext *ctx, JSAtom prop);

    void reset();

    Logger &logger() const { return m_logger; }
    void import(const FileContextBaseConstPtr &fileCtx, JSValue &targetObject,
                ObserveMode observeMode);
    void clearImportsCache();

    void registerEvaluator(Evaluator *evaluator);
    void unregisterEvaluator(const Evaluator *evaluator);
    Evaluator *evaluator() const { return m_evaluator; }

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
    void clearRequestedProperties();
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

    void addImportRequestedInScript(quintptr importValueId);
    std::vector<QString> importedFilesUsedInScript() const;

    void addExternallyCachedValue(JSValue *v) { m_externallyCachedValues.push_back(v); }

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

    void setObservedProperty(JSValue &object, const QString &name, const JSValue &value);
    void unobserveProperties();
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

    JSValue newObject() const;
    JSValue newArray(int length, JsValueOwner owner);
    void takeOwnership(JSValue v);
    JSValue undefinedValue() const { return JS_UNDEFINED; }
    JSValue toScriptValue(const QVariant &v, quintptr id = 0) { return asJsValue(v, id); }
    JSValue evaluate(JsValueOwner resultOwner, const QString &code,
                     const QString &filePath = QString(), int line = 1,
                     const JSValueList &scopeChain = {});
    void setLastLookupStatus(bool success) { m_lastLookupWasSuccess = success; }
    JSContext *context() const { return m_context; }
    JSValue globalObject() const { return m_globalObject; }
    void setGlobalObject(JSValue obj) { m_globalObject = obj; }
    void handleJsProperties(JSValueConst obj, const  PropertyHandler &handler);
    ScopedJsValueList argumentList(const QStringList &argumentNames, const JSValue &context) const;

    using GetProperty = int (*)(JSContext *ctx, JSPropertyDescriptor *desc,
                                JSValueConst obj, JSAtom prop);
    using GetPropertyNames = int (*)(JSContext *ctx, JSPropertyEnum **ptab, uint32_t *plen,
                                     JSValueConst obj);
    JSClassID registerClass(const char *name, JSClassCall *constructor, JSClassFinalizer *finalizer,
                            JSValue scope,
                            GetPropertyNames getPropertyNames = nullptr,
                            GetProperty getProperty = nullptr);
    JSClassID getClassId(const char *name) const;

    JsException checkAndClearException(const CodeLocation &fallbackLocation) const;
    JSValue throwError(const QString &message) const;

    void cancel();

    // The active flag is different from QScriptEngine::isEvaluating.
    // It is set and cleared externally for example by the rule execution code.
    bool isActive() const { return m_active; }
    void setActive(bool on) { m_active = on; }

    JSClassID modulePropertyScriptClass() const;
    void setModulePropertyScriptClass(JSClassID modulePropertyScriptClass);

    JSClassID productPropertyScriptClass() const { return m_productPropertyScriptClass; }
    void setProductPropertyScriptClass(JSClassID productPropertyScriptClass)
    {
        m_productPropertyScriptClass = productPropertyScriptClass;
    }

    JSClassID artifactsScriptClass(int index) const { return m_artifactsScriptClass[index]; }
    void setArtifactsScriptClass(int index, JSClassID artifactsScriptClass)
    {
        m_artifactsScriptClass[index] = artifactsScriptClass;
    }

    JSValue artifactsMapScriptValue(const ResolvedProduct *product);
    void setArtifactsMapScriptValue(const ResolvedProduct *product, JSValue value);
    JSValue artifactsMapScriptValue(const ResolvedModule *module);
    void setArtifactsMapScriptValue(const ResolvedModule *module, JSValue value);

    JSValue getArtifactProperty(JSValue obj,
                                const std::function<JSValue(const Artifact *)> &propGetter);

    JSValue& baseProductScriptValue(const ResolvedProduct *product)
    {
        return m_baseProductScriptValues[product];
    }

    JSValue &projectScriptValue(const ResolvedProject *project)
    {
        return m_projectScriptValues[project];
    }

    JSValue &baseModuleScriptValue(const ResolvedModule *module)
    {
        return m_baseModuleScriptValues[module];
    }

    JSValue getArtifactScriptValue(Artifact *a, const QString &moduleName,
                                   const std::function<void(JSValue obj)> &setup);
    void releaseInputArtifactScriptValues(const RuleNode *ruleNode);

    const JSValueList &contextStack() const { return m_contextStack; }

    JSClassID dataWithPtrClass() const { return m_dataWithPtrClass; }

    JSValue getInternalExtension(const char *name) const;
    void addInternalExtension(const char *name, JSValue ext);
    JSValue asJsValue(const QVariant &v, quintptr id = 0, bool frozen = false);
    JSValue asJsValue(const QByteArray &s);
    JSValue asJsValue(const QString &s);
    JSValue asJsValue(const QStringList &l);
    JSValue asJsValue(const QVariantList &l, quintptr id = 0, bool frozen = false);
    JSValue asJsValue(const QVariantMap &m, quintptr id = 0, bool frozen = false);

    QVariant property(const char *name) const { return m_properties.value(QLatin1String(name)); }
    void setProperty(const char *k, const QVariant &v) { m_properties.insert(QLatin1String(k), v); }

private:
    class Importer {
    public:
        Importer(ScriptEngine &engine, const FileContextBaseConstPtr &fileCtx,
                 JSValue &targetObject, ObserveMode observeMode);
        ~Importer();
        void run();

    private:
        ScriptEngine &m_engine;
        const FileContextBaseConstPtr &m_fileCtx;
        JSValue &m_targetObject;
    };

    static int interruptor(JSRuntime *rt, void *opaqueEngine);

    bool gatherFileResults() const;

    void setMaxStackSize();
    void setPropertyOnGlobalObject(const QString &property, JSValue value);
    void installQbsBuiltins();
    void extendJavaScriptBuiltins();
    void installConsoleFunction(JSValue consoleObj, const QString &name, LoggerLevel level);
    void installImportFunctions(JSValue importScope);
    void uninstallImportFunctions();
    void import(const JsImport &jsImport, JSValue &targetObject);
    void observeImport(JSValue &jsImport);
    void importFile(const QString &filePath, JSValue targetObject);
    static JSValue js_require(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv, int magic, JSValue *func_data);
    JSValue mergeExtensionObjects(const JSValueList &lst);
    JSValue loadInternalExtension(const QString &uri);

    static void handleUndefinedFound(JSContext *ctx);
    static void handleFunctionEntered(JSContext *ctx, JSValue this_obj);
    static void handleFunctionExited(JSContext *ctx);

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
        friend QHashValueType qHash(const ScriptEngine::PropertyCacheKey &k, QHashValueType seed);
    };

    friend bool operator==(const PropertyCacheKey &lhs, const PropertyCacheKey &rhs);
    friend QHashValueType qHash(const ScriptEngine::PropertyCacheKey &k, QHashValueType seed);

    JSRuntime * const m_jsRuntime = JS_NewRuntime();
    JSContext * const m_context = JS_NewContext(m_jsRuntime);
    JSValue m_globalObject = JS_NULL;
    ScriptImporter *m_scriptImporter;
    JSClassID m_modulePropertyScriptClass = 0;
    JSClassID m_productPropertyScriptClass = 0;
    JSClassID m_artifactsScriptClass[2] = {0, 0};
    JSClassID m_dataWithPtrClass = 0;
    Evaluator *m_evaluator = nullptr;
    QHash<JsImport, JSValue> m_jsImportCache;
    std::unordered_map<QString, JSValue> m_jsFileCache;
    bool m_propertyCacheEnabled = true;
    bool m_active = false;
    std::atomic_bool m_canceling = false;
    QHash<PropertyCacheKey, QVariant> m_propertyCache;
    PropertySet m_propertiesRequestedInScript;
    QHash<QString, PropertySet> m_propertiesRequestedFromArtifact;
    Logger &m_logger;
    QProcessEnvironment m_environment;
    QHash<QString, QString> m_canonicalFilePathResult;
    QHash<QString, bool> m_fileExistsResult;
    QHash<std::pair<QString, quint32>, QStringList> m_directoryEntriesResult;
    QHash<QString, FileTime> m_fileLastModifiedResult;
    std::stack<QString> m_currentDirPathStack;
    std::stack<QStringList> m_extensionSearchPathsStack;
    std::stack<std::pair<QString, int>> m_evalPositions;
    JSValue m_qbsObject = JS_UNDEFINED;
    qint64 m_elapsedTimeImporting = -1;
    bool m_usesIo = false;
    EvalContext m_evalContext;
    const std::unique_ptr<PrepareScriptObserver> m_observer;
    std::vector<std::tuple<JSValue, QString, JSValue>> m_observedProperties;
    JSValueList m_requireResults;
    std::unordered_map<quintptr, std::vector<QString>> m_filePathsPerImport;
    std::vector<qint64> m_importsRequestedInScript;
    Set<const ResolvedProduct *> m_productsWithRequestedDependencies;
    RequestedArtifacts m_requestedArtifacts;
    Set<const ResolvedProduct *> m_requestedExports;
    ObserveMode m_observeMode = ObserveMode::Disabled;
    std::unordered_map<const ResolvedProduct *, JSValue> m_baseProductScriptValues;
    std::unordered_map<const ResolvedProduct *, JSValue> m_productArtifactsMapScriptValues;
    std::unordered_map<const ResolvedModule *, JSValue> m_moduleArtifactsMapScriptValues;
    std::unordered_map<const ResolvedProject *, JSValue> m_projectScriptValues;
    std::unordered_map<const ResolvedModule *, JSValue> m_baseModuleScriptValues;
    QList<JSValueList> m_scopeChains;
    JSValueList m_contextStack;
    QHash<JSClassID, JSClassExoticMethods> m_exoticMethods;
    QHash<QString, JSClassID> m_classes;
    QHash<QString, JSValue> m_internalExtensions;
    QHash<QString, JSValue> m_stringCache;
    QHash<quintptr, JSValue> m_jsValueCache;
    QHash<JSValue, int> m_evalResults;
    std::vector<JSValue *> m_externallyCachedValues;
    QHash<QPair<Artifact *, QString>, JSValue> m_artifactsScriptValues;
    QVariantMap m_properties;
    std::recursive_mutex m_artifactsMutex;
    bool m_lastLookupWasSuccess = false;
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
