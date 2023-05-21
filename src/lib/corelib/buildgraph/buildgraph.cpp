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
#include "buildgraph.h"

#include "artifact.h"
#include "artifactsscriptvalue.h"
#include "cycledetector.h"
#include "dependencyparametersscriptvalue.h"
#include "projectbuilddata.h"
#include "productbuilddata.h"
#include "rulenode.h"
#include "transformer.h"

#include <jsextensions/jsextensions.h>
#include <jsextensions/moduleproperties.h>
#include <language/artifactproperties.h>
#include <language/language.h>
#include <language/preparescriptobserver.h>
#include <language/propertymapinternal.h>
#include <language/resolvedfilecontext.h>
#include <language/scriptengine.h>
#include <logging/categories.h>
#include <logging/logger.h>
#include <language/property.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/scripttools.h>
#include <tools/stlutils.h>
#include <tools/stringconstants.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>

#include <algorithm>
#include <iterator>
#include <vector>

namespace qbs {
namespace Internal {

static QString childItemsProperty() { return QStringLiteral("childItems"); }
static QString exportsProperty() { return QStringLiteral("exports"); }

// TODO: Introduce productscriptvalue.{h,cpp}.

static JSValue getDataObject(JSContext *ctx, JSValue obj)
{
    return getJsProperty(ctx, obj, StringConstants::dataPropertyInternal());
}

static JSValue getValueFromData(JSContext *ctx, JSValue data,
                                ModulePropertiesScriptValueCommonPropertyKeys key)
{
    return JS_GetPropertyUint32(ctx, data, key);
}

static JSValue getValueFromObject(JSContext *ctx, JSValue obj,
                                ModulePropertiesScriptValueCommonPropertyKeys key)
{
    const ScopedJsValue data(ctx, getDataObject(ctx, obj));
    return getValueFromData(ctx, data, key);
}

void getPropertyNames(JSContext *ctx, JSPropertyEnum **ptab, uint32_t *plen,
                      const QVariantMap &properties, const QStringList &extraPropertyNames,
                      JSValue extraObject)
{
    JSPropertyEnum *basePTab = nullptr;
    uint32_t basePlen = 0;
    JS_GetOwnPropertyNames(ctx, &basePTab, &basePlen, extraObject,
                           JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY);
    *plen = uint32_t(extraPropertyNames.size() + properties.size() + basePlen);
    *ptab = reinterpret_cast<JSPropertyEnum *>(js_malloc(ctx, *plen * sizeof **ptab));
    JSPropertyEnum *entry = *ptab;
    for (auto it = properties.begin(); it != properties.end(); ++it, ++entry) {
        entry->atom = JS_NewAtom(ctx, it.key().toUtf8().constData());
        entry->is_enumerable = 1;
    }
    for (const QString &prop : extraPropertyNames) {
        entry->atom = JS_NewAtom(ctx, prop.toUtf8().constData());
        entry->is_enumerable = 1;
        ++entry;
    }
    for (uint32_t i = 0; i < basePlen; ++i, ++entry) {
        entry->atom = basePTab[i].atom;
        entry->is_enumerable = 1;
    }
    js_free(ctx, basePTab);
}

static int getProductPropertyNames(JSContext *ctx, JSPropertyEnum **ptab, uint32_t *plen,
                                   JSValueConst obj)
{
    ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
    const ScopedJsValue data(ctx, getDataObject(engine->context(), obj));
    const auto product = attachedPointer<ResolvedProduct>(
                obj, engine->productPropertyScriptClass());
    QBS_ASSERT(product, return -1);

    // The "moduleName" convenience property is only available for the "main product" in a rule,
    // and the "parameters" property exists only for elements of the "dependencies" array for
    // which dependency parameters are present.
    const auto hasModuleName = [&] {
        const ScopedJsValue v(ctx, getValueFromData(ctx, data, ModuleNameKey));
        return JS_IsString(v);
    };
    const auto hasDependencyParams = [&] {
        const ScopedJsValue v(ctx, getValueFromData(ctx, data, DependencyParametersKey));
        return JS_IsObject(v);
    };
    QStringList additionalProperties;
    if (hasModuleName())
        additionalProperties.push_back(StringConstants::moduleNameProperty());
    else if (hasDependencyParams())
        additionalProperties.push_back(StringConstants::parametersProperty());
    getPropertyNames(ctx, ptab, plen, product->productProperties, additionalProperties,
                     engine->baseProductScriptValue(product));
    return 0;
}

static int getProductProperty(JSContext *ctx, JSPropertyDescriptor *desc, JSValueConst obj,
                              JSAtom prop)
{
    if (desc) {
        desc->getter = desc->setter = desc->value = JS_UNDEFINED;
        desc->flags = JS_PROP_ENUMERABLE;
    }
    const QString name = getJsString(ctx, prop);
    if (name == StringConstants::parametersProperty()) {
        if (desc)
            desc->value = getValueFromObject(ctx, obj, DependencyParametersKey);
        return 1;
    }
    if (name == StringConstants::moduleNameProperty()) {
        if (desc)
            desc->value = getValueFromObject(ctx, obj, ModuleNameKey);
        return 1;
    }

    ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
    const auto product = attachedPointer<ResolvedProduct>(
                obj, engine->productPropertyScriptClass());
    QBS_ASSERT(product, return -1);

    const JSValue baseProductValue = engine->baseProductScriptValue(product);
    if (name == exportsProperty()) {
        if (desc)
            desc->value = getJsProperty(ctx, baseProductValue, name);
        engine->addRequestedExport(product);
        return 1;
    }

    const auto it = product->productProperties.constFind(name);
    if (it == product->productProperties.cend()) {
        ScopedJsValue v(ctx, JS_GetProperty(ctx, baseProductValue, prop));
        const int ret = JS_IsUndefined(v) ? 0 : 1;
        if (desc)
            desc->value = v.release();
        return ret;
    }

    engine->addPropertyRequestedInScript(Property(product->uniqueName(), QString(), name,
                                                  it.value(), Property::PropertyInProduct));
    if (desc)
        desc->value = engine->toScriptValue(it.value());
    return 1;
}

static JSValue setupProjectScriptValue(ScriptEngine *engine, const ResolvedProjectConstPtr &project)
{
    JSValue &obj = engine->projectScriptValue(project.get());
    if (JS_IsObject(obj))
        return JS_DupValue(engine->context(), obj);
    obj = engine->newObject();
    setJsProperty(engine->context(), obj, StringConstants::filePathProperty(),
                  project->location.filePath());
    setJsProperty(engine->context(), obj, StringConstants::pathProperty(),
                  FileInfo::path(project->location.filePath()));
    const QVariantMap &projectProperties = project->projectProperties();
    for (QVariantMap::const_iterator it = projectProperties.begin();
            it != projectProperties.end(); ++it) {
        const ScopedJsValue val(engine->context(), engine->toScriptValue(it.value()));
        engine->setObservedProperty(obj, it.key(), val);
    }
    engine->observer()->addProjectObjectId(jsObjectId(obj), project->name);
    return JS_DupValue(engine->context(), obj);
}

static void setupBaseProductScriptValue(ScriptEngine *engine, const ResolvedProduct *product);

class DependenciesFunction
{
public:
    DependenciesFunction(ScriptEngine *engine) : m_engine(engine) { }

    void init(JSValue &productScriptValue, JSValue exportsScriptValue)
    {
        const QByteArray name = StringConstants::dependenciesProperty().toUtf8();
        const ScopedJsValue depfunc(
                    m_engine->context(),
                    JS_NewCFunction(m_engine->context(), &js_internalProductDependencies,
                                    name.constData(), 0));
        const ScopedJsAtom depAtom(m_engine->context(), name);
        JS_DefineProperty(m_engine->context(), productScriptValue, depAtom, JS_UNDEFINED, depfunc,
                          JS_UNDEFINED, JS_PROP_HAS_GET | JS_PROP_ENUMERABLE);
        const ScopedJsValue exportedDepfunc(
                    m_engine->context(),
                    JS_NewCFunction(m_engine->context(), &js_exportedProductDependencies,
                                    name.constData(), 0));
        JS_DefineProperty(m_engine->context(), exportsScriptValue, depAtom, JS_UNDEFINED,
                          exportedDepfunc, JS_UNDEFINED, JS_PROP_HAS_GET | JS_PROP_ENUMERABLE);
    }

private:
    enum class DependencyType { Internal, Exported };
    static JSValue js_productDependencies(const ResolvedProduct *product,
                                          JSContext *ctx, JSValueConst this_val, int argc,
                                          JSValueConst *argv, DependencyType depType)
    {
        Q_UNUSED(this_val)
        Q_UNUSED(argc)
        Q_UNUSED(argv)

        ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
        JSValue result = JS_NewArray(ctx);
        quint32 idx = 0;
        const bool exportCase = depType == DependencyType::Exported;
        std::vector<ResolvedProductPtr> productDeps;
        if (exportCase) {
            if (!product->exportedModule.productDependencies.empty()) {
                const auto allProducts = product->topLevelProject()->allProducts();
                const auto getProductForName = [&allProducts](const QString &name) {
                    const auto cmp = [name](const ResolvedProductConstPtr &p) {
                        return p->uniqueName() == name;
                    };
                    const auto it = std::find_if(allProducts.cbegin(), allProducts.cend(), cmp);
                    QBS_ASSERT(it != allProducts.cend(), return ResolvedProductPtr());
                    return *it;
                };
                std::transform(product->exportedModule.productDependencies.cbegin(),
                               product->exportedModule.productDependencies.cend(),
                               std::back_inserter(productDeps), getProductForName);
            }
        } else {
            productDeps = product->dependencies;
        }
        for (const ResolvedProductPtr &dependency : std::as_const(productDeps)) {
            setupBaseProductScriptValue(engine, dependency.get());
            JSValue obj = JS_NewObjectClass(engine->context(),
                                            engine->productPropertyScriptClass());
            attachPointerTo(obj, dependency.get());
            const QVariantMap &params
                    = (exportCase ? product->exportedModule.dependencyParameters.value(dependency)
                                  : product->dependencyParameters.value(dependency));
            JSValue data = JS_NewObjectClass(engine->context(), engine->dataWithPtrClass());
            JS_SetPropertyUint32(ctx, data, DependencyParametersKey, dependencyParametersValue(
                                     product->uniqueName(), dependency->name, params, engine));
            defineJsProperty(ctx, obj, StringConstants::dataPropertyInternal(), data);
            JS_SetPropertyUint32(ctx, result, idx++, obj);
        }
        if (exportCase) {
            for (const ExportedModuleDependency &m : product->exportedModule.moduleDependencies) {
                JSValue obj = engine->newObject();
                setJsProperty(ctx, obj, StringConstants::nameProperty(), m.name);
                JSValue exportsValue = engine->newObject();
                setJsProperty(ctx, obj, exportsProperty(), exportsValue);
                setJsProperty(ctx, exportsValue, StringConstants::dependenciesProperty(),
                              JS_NewArray(ctx));
                for (auto modIt = m.moduleProperties.begin(); modIt != m.moduleProperties.end();
                     ++modIt) {
                    const QVariantMap entries = modIt.value().toMap();
                    if (entries.empty())
                        continue;
                    JSValue moduleObj = engine->newObject();
                    ModuleProperties::setModuleScriptValue(engine, exportsValue, moduleObj,
                                                           modIt.key());
                    for (auto valIt = entries.begin(); valIt != entries.end(); ++valIt) {
                        setJsProperty(ctx, moduleObj, valIt.key(),
                                      engine->toScriptValue(valIt.value()));
                    }
                }
                JS_SetPropertyUint32(ctx, result, idx++, obj);
            }
            return result;
        }
        for (const auto &dependency : product->modules) {
            if (dependency->isProduct)
                continue;
            JSValue obj = JS_NewObjectClass(ctx, engine->modulePropertyScriptClass());
            attachPointerTo(obj, dependency.get());
            const QVariantMap &params = product->moduleParameters.value(dependency);
            JSValue data = createDataForModuleScriptValue(engine, nullptr);
            JS_SetPropertyUint32(ctx, data, DependencyParametersKey, dependencyParametersValue(
                                     product->uniqueName(), dependency->name, params, engine));
            defineJsProperty(ctx, obj, StringConstants::dataPropertyInternal(), data);
            JS_SetPropertyUint32(ctx, result, idx++, obj);
        }
        return result;
    }

    static JSValue js_internalProductDependencies(JSContext *ctx, JSValueConst this_val,
                                                  int argc, JSValueConst *argv)
    {
        ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
        const auto product = attachedPointer<ResolvedProduct>(this_val, engine->dataWithPtrClass());
        engine->addDependenciesArrayRequested(product);
        return js_productDependencies(product, ctx, this_val, argc, argv,
                                      DependencyType::Internal);
    }

    static JSValue js_exportedProductDependencies(JSContext *ctx, JSValueConst this_val,
                                                  int argc, JSValueConst *argv)
    {
        const auto product = attachedPointer<ResolvedProduct>(
                    this_val, ScriptEngine::engineForContext(ctx)->dataWithPtrClass());
        return js_productDependencies(product, ctx, this_val, argc, argv, DependencyType::Exported);
    }

    ScriptEngine *m_engine;
};

static JSValue setupExportedPropertyScriptValue(const ExportedProperty &property,
                                                     ScriptEngine *engine)
{
    JSValue propertyScriptValue = engine->newObject();
    setJsProperty(engine->context(), propertyScriptValue, StringConstants::nameProperty(),
                  property.fullName);
    setJsProperty(engine->context(), propertyScriptValue, StringConstants::typeProperty(),
                  PropertyDeclaration::typeString(property.type));
    setJsProperty(engine->context(), propertyScriptValue, StringConstants::sourceCodeProperty(),
                  property.sourceCode);
    JS_SetPropertyStr(engine->context(), propertyScriptValue, "isBuiltin",
                      JS_NewBool(engine->context(), property.isBuiltin));
    return propertyScriptValue;
}

static void setupExportedPropertiesScriptValue(JSValue &parentObject,
                                               const std::vector<ExportedProperty> &properties,
                                               ScriptEngine *engine)
{
    JSValue propertiesScriptValue = JS_NewArray(engine->context());
    quint32 arrayIndex = 0;
    for (const ExportedProperty &p : properties) {
        JS_SetPropertyUint32(engine->context(), propertiesScriptValue, arrayIndex++,
                             setupExportedPropertyScriptValue(p, engine));
    }
    JS_SetPropertyStr(engine->context(), parentObject, "properties", propertiesScriptValue);
}

static JSValue setupExportedItemScriptValue(const ExportedItem *item, ScriptEngine *engine)
{
    JSValue itemScriptValue = engine->newObject();
    setJsProperty(engine->context(), itemScriptValue, StringConstants::nameProperty(), item->name);
    setupExportedPropertiesScriptValue(itemScriptValue, item->properties, engine);
    JSValue childrenScriptValue = JS_NewArray(engine->context());
    quint32 arrayIndex = 0;
    for (const auto &childItem : item->children) {
        JS_SetPropertyUint32(engine->context(), childrenScriptValue, arrayIndex++,
                             setupExportedItemScriptValue(childItem.get(), engine));
    }
    setJsProperty(engine->context(), itemScriptValue, childItemsProperty(), childrenScriptValue);
    return itemScriptValue;
}

static JSValue setupExportsScriptValue(const ResolvedProduct *product, ScriptEngine *engine)
{
    const ExportedModule &module = product->exportedModule;
    JSValue exportsScriptValue = JS_NewObjectClass(engine->context(), engine->dataWithPtrClass());
    attachPointerTo(exportsScriptValue, product);
    for (auto it = module.propertyValues.cbegin(); it != module.propertyValues.cend(); ++it) {
        setJsProperty(engine->context(), exportsScriptValue, it.key(),
                      engine->toScriptValue(it.value()));
    }
    setupExportedPropertiesScriptValue(exportsScriptValue, module.m_properties, engine);
    JSValue childrenScriptValue = JS_NewArray(engine->context());
    quint32 arrayIndex = 0;
    for (const auto &exportedItem : module.children) {
        JS_SetPropertyUint32(engine->context(), childrenScriptValue, arrayIndex++,
                             setupExportedItemScriptValue(exportedItem.get(), engine));
    }
    setJsProperty(engine->context(), exportsScriptValue, childItemsProperty(), childrenScriptValue);
    JSValue importsScriptValue = JS_NewArray(engine->context());
    arrayIndex = 0;
    for (const QString &importStatement : module.importStatements) {
        JS_SetPropertyUint32(engine->context(), importsScriptValue, arrayIndex++,
                             makeJsString(engine->context(), importStatement));
    }
    setJsProperty(engine->context(), exportsScriptValue, StringConstants::importsProperty(),
                  importsScriptValue);
    for (auto it = module.modulePropertyValues.cbegin(); it != module.modulePropertyValues.cend();
         ++it) {
        const QVariantMap entries = it.value().toMap();
        if (entries.empty())
            continue;
        JSValue moduleObject = engine->newObject();
        ModuleProperties::setModuleScriptValue(engine, exportsScriptValue, moduleObject, it.key());
        for (auto valIt = entries.begin(); valIt != entries.end(); ++valIt) {
            setJsProperty(engine->context(), moduleObject, valIt.key(),
                          engine->toScriptValue(valIt.value()));
        }
    }
    return exportsScriptValue;
}

static void setupBaseProductScriptValue(ScriptEngine *engine, const ResolvedProduct *product)
{
    JSValue &productScriptValue = engine->baseProductScriptValue(product);
    if (JS_IsObject(productScriptValue))
        return;
    const ScopedJsValue proto(engine->context(), JS_NewObject(engine->context()));
    productScriptValue = JS_NewObjectProtoClass(engine->context(), proto,
                                                engine->dataWithPtrClass());
    attachPointerTo(productScriptValue, product);
    ModuleProperties::init(engine, productScriptValue, product);

    const QByteArray funcName = StringConstants::artifactsProperty().toUtf8();
    const ScopedJsValue artifactsFunc(
                engine->context(),
                JS_NewCFunction(engine->context(), &artifactsScriptValueForProduct,
                                funcName.constData(), 0));
    const ScopedJsAtom artifactsAtom(engine->context(), funcName);
    JS_DefineProperty(engine->context(), productScriptValue, artifactsAtom,
                      JS_UNDEFINED, artifactsFunc, JS_UNDEFINED,
                      JS_PROP_HAS_GET | JS_PROP_ENUMERABLE);

    // FIXME: Proper un-observe rather than ref count decrease here.
    ScopedJsValue exportsScriptValue(engine->context(), setupExportsScriptValue(product, engine));
    DependenciesFunction(engine).init(productScriptValue, exportsScriptValue);

    // TODO: Why are these necessary? We catch accesses to product.exports in getProductProperty().
    // But the exportsQbs() and exportsPkgConfig() tests fail without them.
    engine->setObservedProperty(productScriptValue, exportsProperty(), exportsScriptValue);
    engine->observer()->addExportsObjectId(jsObjectId(exportsScriptValue), product);
}

void setupScriptEngineForFile(ScriptEngine *engine, const FileContextBaseConstPtr &fileContext,
        JSValue targetObject, const ObserveMode &observeMode)
{
    engine->import(fileContext, targetObject, observeMode);
    JsExtensions::setupExtensions(engine, fileContext->jsExtensions(), targetObject);
}

void setupScriptEngineForProduct(ScriptEngine *engine, ResolvedProduct *product,
                                 const ResolvedModule *module, JSValue targetObject,
                                 bool setBuildEnvironment)
{
    JSValue projectScriptValue = setupProjectScriptValue(engine, product->project.lock());
    setJsProperty(engine->context(), targetObject, StringConstants::projectVar(),
                  projectScriptValue);
    if (setBuildEnvironment) {
        QVariant v;
        v.setValue<void*>(&product->buildEnvironment);
        engine->setProperty(StringConstants::qbsProcEnvVarInternal(), v);
    }
    JSClassID scriptClass = engine->productPropertyScriptClass();
    if (scriptClass == 0) {
        engine->setProductPropertyScriptClass(engine->registerClass("ProductProperties", nullptr,
                nullptr, JS_UNDEFINED, &getProductPropertyNames, &getProductProperty));
        scriptClass = engine->productPropertyScriptClass();
    }
    setupBaseProductScriptValue(engine, product);
    JSValue productScriptValue = JS_NewObjectClass(engine->context(), scriptClass);
    attachPointerTo(productScriptValue, product);
    setJsProperty(engine->context(), targetObject, StringConstants::productVar(),
                  productScriptValue);

    JSValue data = JS_NewObjectClass(engine->context(), engine->dataWithPtrClass());
    // If the Rule is in a Module, set up the 'moduleName' property
    if (!module->name.isEmpty()) {
        JS_SetPropertyUint32(engine->context(), data, ModuleNameKey,
                             makeJsString(engine->context(), module->name));
    }
    defineJsProperty(engine->context(), productScriptValue,
                     StringConstants::dataPropertyInternal(), data);
}

bool findPath(BuildGraphNode *u, BuildGraphNode *v, QList<BuildGraphNode *> &path)
{
    if (u == v) {
        path.push_back(v);
        return true;
    }

    for (BuildGraphNode * const childNode : std::as_const(u->children)) {
        if (findPath(childNode, v, path)) {
            path.prepend(u);
            return true;
        }
    }

    return false;
}

/*
 * Creates the build graph edge p -> c, which represents the dependency "c must be built before p".
 */
void connect(BuildGraphNode *p, BuildGraphNode *c)
{
    QBS_CHECK(p != c);
    qCDebug(lcBuildGraph).noquote() << "connect" << p->toString() << "->" << c->toString();
    if (c->type() == BuildGraphNode::ArtifactNodeType) {
        auto const ac = static_cast<Artifact *>(c);
        for (const Artifact *child : filterByType<Artifact>(p->children)) {
            if (child == ac)
                return;
            const bool filePathsMustBeDifferent = child->artifactType == Artifact::Generated
                    || child->product == ac->product || child->artifactType != ac->artifactType;
            if (filePathsMustBeDifferent && child->filePath() == ac->filePath()) {
                throw ErrorInfo(QStringLiteral("%1 already has a child artifact %2 as "
                                                    "different object.").arg(p->toString(),
                                                                             ac->filePath()),
                                CodeLocation(), true);
            }
        }
    }
    p->children.insert(c);
    c->parents.insert(p);
    p->product->topLevelProject()->buildData->setDirty();
}

static bool existsPath_impl(BuildGraphNode *u, BuildGraphNode *v, NodeSet *seen)
{
    if (u == v)
        return true;

    if (!seen->insert(u).second)
        return false;

    return Internal::any_of(u->children, [v, seen](const auto &child) {
        return existsPath_impl(child, v, seen);
    });
}

static bool existsPath(BuildGraphNode *u, BuildGraphNode *v)
{
    NodeSet seen;
    return existsPath_impl(u, v, &seen);
}

static QStringList toStringList(const QList<BuildGraphNode *> &path)
{
    QStringList lst;
    for (BuildGraphNode *node : path)
        lst << node->toString();
    return lst;
}

bool safeConnect(Artifact *u, Artifact *v)
{
    QBS_CHECK(u != v);
    qCDebug(lcBuildGraph) << "safeConnect:" << relativeArtifactFileName(u)
                          << "->" << relativeArtifactFileName(v);

    if (existsPath(v, u)) {
        QList<BuildGraphNode *> circle;
        findPath(v, u, circle);
        qCDebug(lcBuildGraph) << "safeConnect: circle detected " << toStringList(circle);
        return false;
    }

    connect(u, v);
    return true;
}

void disconnect(BuildGraphNode *u, BuildGraphNode *v)
{
    qCDebug(lcBuildGraph).noquote() << "disconnect:" << u->toString() << v->toString();
    u->children.remove(v);
    v->parents.remove(u);
    u->onChildDisconnected(v);
}

void removeGeneratedArtifactFromDisk(Artifact *artifact, const Logger &logger)
{
    if (artifact->artifactType != Artifact::Generated)
        return;
    removeGeneratedArtifactFromDisk(artifact->filePath(), logger);
}

void removeGeneratedArtifactFromDisk(const QString &filePath, const Logger &logger)
{
    QFile file(filePath);
    if (!file.exists())
        return;
    logger.qbsDebug() << "removing " << filePath;
    if (!file.remove())
        logger.qbsWarning() << QStringLiteral("Cannot remove '%1'.").arg(filePath);
}

QString relativeArtifactFileName(const Artifact *artifact)
{
    const QString &buildDir = artifact->product->topLevelProject()->buildDirectory;
    QString str = artifact->filePath();
    if (str.startsWith(buildDir))
        str.remove(0, buildDir.size());
    if (str.startsWith(QLatin1Char('/')))
        str.remove(0, 1);
    return str;
}

Artifact *lookupArtifact(const ResolvedProductConstPtr &product,
        const ProjectBuildData *projectBuildData, const QString &dirPath, const QString &fileName,
        bool compareByName)
{
    for (const auto &fileResource : projectBuildData->lookupFiles(dirPath, fileName)) {
        if (fileResource->fileType() != FileResourceBase::FileTypeArtifact)
            continue;
        const auto artifact = static_cast<Artifact *>(fileResource);
        if (compareByName
                ? artifact->product->uniqueName() == product->uniqueName()
                : artifact->product == product) {
            return artifact;
        }
    }
    return nullptr;
}

Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const QString &dirPath,
                         const QString &fileName, bool compareByName)
{
    return lookupArtifact(product, product->topLevelProject()->buildData.get(), dirPath, fileName,
                          compareByName);
}

Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const QString &filePath,
                         bool compareByName)
{
    QString dirPath, fileName;
    FileInfo::splitIntoDirectoryAndFileName(filePath, &dirPath, &fileName);
    return lookupArtifact(product, dirPath, fileName, compareByName);
}

Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const ProjectBuildData *buildData,
                         const QString &filePath, bool compareByName)
{
    QString dirPath, fileName;
    FileInfo::splitIntoDirectoryAndFileName(filePath, &dirPath, &fileName);
    return lookupArtifact(product, buildData, dirPath, fileName, compareByName);
}

Artifact *lookupArtifact(const ResolvedProductConstPtr &product, const Artifact *artifact,
                         bool compareByName)
{
    return lookupArtifact(product, artifact->dirPath(), artifact->fileName(), compareByName);
}

Artifact *createArtifact(const ResolvedProductPtr &product,
                         const SourceArtifactConstPtr &sourceArtifact)
{
    const auto artifact = new Artifact;
    artifact->artifactType = Artifact::SourceFile;
    setArtifactData(artifact, sourceArtifact);
    insertArtifact(product, artifact);
    return artifact;
}

void setArtifactData(Artifact *artifact, const SourceArtifactConstPtr &sourceArtifact)
{
    artifact->targetOfModule = sourceArtifact->targetOfModule;
    artifact->setFilePath(sourceArtifact->absoluteFilePath);
    artifact->setFileTags(sourceArtifact->fileTags);
    artifact->properties = sourceArtifact->properties;
}

void updateArtifactFromSourceArtifact(const ResolvedProductPtr &product,
                                      const SourceArtifactConstPtr &sourceArtifact)
{
    Artifact * const artifact = lookupArtifact(product, sourceArtifact->absoluteFilePath, false);
    QBS_CHECK(artifact);
    const FileTags oldFileTags = artifact->fileTags();
    const QVariantMap oldModuleProperties = artifact->properties->value();
    setArtifactData(artifact, sourceArtifact);
    if (oldFileTags != artifact->fileTags()
            || oldModuleProperties != artifact->properties->value()) {
        invalidateArtifactAsRuleInputIfNecessary(artifact);
    }
}

void insertArtifact(const ResolvedProductPtr &product, Artifact *artifact)
{
    qCDebug(lcBuildGraph) << "insert artifact" << artifact->filePath();
    QBS_CHECK(!artifact->product);
    QBS_CHECK(!artifact->filePath().isEmpty());
    artifact->product = product;
    product->topLevelProject()->buildData->insertIntoLookupTable(artifact);
    product->buildData->addArtifact(artifact);
}

void provideFullFileTagsAndProperties(Artifact *artifact)
{
    artifact->properties = artifact->product->moduleProperties;
    FileTags allTags = artifact->pureFileTags.empty()
            ? artifact->product->fileTagsForFileName(artifact->fileName()) : artifact->pureFileTags;
    for (const auto &props : artifact->product->artifactProperties) {
        if (allTags.intersects(props->fileTagsFilter())) {
            artifact->properties = props->propertyMap();
            allTags += props->extraFileTags();
            break;
        }
    }
    artifact->setFileTags(allTags);

    // Let a positive value of qbs.install imply the file tag "installable".
    if (artifact->properties->qbsPropertyValue(StringConstants::installProperty()).toBool())
        artifact->addFileTag("installable");
}

void applyPerArtifactProperties(Artifact *artifact)
{
    if (artifact->pureProperties.empty())
        return;
    QVariantMap props = artifact->properties->value();
    for (const auto &property : artifact->pureProperties)
        setConfigProperty(props, property.first, property.second);
    artifact->properties = artifact->properties->clone();
    artifact->properties->setValue(props);
}

void updateGeneratedArtifacts(ResolvedProduct *product)
{
    if (!product->buildData)
        return;
    for (Artifact * const artifact : filterByType<Artifact>(product->buildData->allNodes())) {
        if (artifact->artifactType == Artifact::Generated) {
            const FileTags oldFileTags = artifact->fileTags();
            const QVariantMap oldModuleProperties = artifact->properties->value();
            provideFullFileTagsAndProperties(artifact);
            applyPerArtifactProperties(artifact);
            if (oldFileTags != artifact->fileTags()
                    || oldModuleProperties != artifact->properties->value()) {
                invalidateArtifactAsRuleInputIfNecessary(artifact);
            }
        }
    }
}

// This is needed for artifacts which are inputs to rules whose outputArtifacts script
// returned an empty array for this input. Since there is no transformer, our usual change
// tracking procedure will not notice if the artifact's file tags or module properties have
// changed, so we need to force a re-run of the outputArtifacts script.
void invalidateArtifactAsRuleInputIfNecessary(Artifact *artifact)
{
    for (RuleNode * const parentRuleNode : filterByType<RuleNode>(artifact->parents)) {
        if (!parentRuleNode->rule()->isDynamic())
            continue;
        bool artifactNeedsExplicitInvalidation = true;
        for (Artifact * const output : filterByType<Artifact>(parentRuleNode->parents)) {
            if (output->children.contains(artifact)
                    && !output->childrenAddedByScanner.contains(artifact)) {
                artifactNeedsExplicitInvalidation = false;
                break;
            }
        }
        if (artifactNeedsExplicitInvalidation)
            parentRuleNode->removeOldInputArtifact(artifact);
    }
}

static void doSanityChecksForProduct(const ResolvedProductConstPtr &product,
        const Set<ResolvedProductPtr> &allProducts, const Logger &logger)
{
    qCDebug(lcBuildGraph) << "Sanity checking product" << product->uniqueName();
    CycleDetector cycleDetector(logger);
    cycleDetector.visitProduct(product);
    const ProductBuildData * const buildData = product->buildData.get();
    for (const auto &m : product->modules)
        QBS_CHECK(m->product == product.get());
    qCDebug(lcBuildGraph) << "enabled:" << product->enabled << "build data:" << buildData;
    if (product->enabled)
        QBS_CHECK(buildData);
    if (!product->buildData)
        return;
    for (BuildGraphNode * const node : std::as_const(buildData->rootNodes())) {
        qCDebug(lcBuildGraph).noquote() << "Checking root node" << node->toString();
        QBS_CHECK(buildData->allNodes().contains(node));
    }
    Set<QString> filePaths;
    for (BuildGraphNode * const node : std::as_const(buildData->allNodes())) {
        qCDebug(lcBuildGraph).noquote() << "Sanity checking node" << node->toString();
        QBS_CHECK(node->product == product);
        for (const BuildGraphNode * const parent : std::as_const(node->parents))
            QBS_CHECK(parent->children.contains(node));
        for (BuildGraphNode * const child : std::as_const(node->children)) {
            QBS_CHECK(child->parents.contains(node));
            QBS_CHECK(!child->product.expired());
            QBS_CHECK(child->product->buildData);
            QBS_CHECK(child->product->buildData->allNodes().contains(child));
            QBS_CHECK(allProducts.contains(child->product.lock()));
        }

        Artifact * const artifact = node->type() == BuildGraphNode::ArtifactNodeType
                ? static_cast<Artifact *>(node) : nullptr;
        if (!artifact) {
            QBS_CHECK(node->type() == BuildGraphNode::RuleNodeType);
            auto const ruleNode = static_cast<RuleNode *>(node);
            QBS_CHECK(ruleNode->rule());
            QBS_CHECK(ruleNode->rule()->product);
            QBS_CHECK(ruleNode->rule()->product == ruleNode->product.get());
            QBS_CHECK(ruleNode->rule()->product == product.get());
            QBS_CHECK(contains(product->rules, std::const_pointer_cast<Rule>(ruleNode->rule())));
            continue;
        }

        QBS_CHECK(product->topLevelProject()->buildData->fileDependencies.contains(
                      artifact->fileDependencies));
        QBS_CHECK(artifact->artifactType == Artifact::SourceFile ||
                  !filePaths.contains(artifact->filePath()));
        filePaths << artifact->filePath();

        for (Artifact * const child : std::as_const(artifact->childrenAddedByScanner))
            QBS_CHECK(artifact->children.contains(child));
        const TransformerConstPtr transformer = artifact->transformer;
        if (artifact->artifactType == Artifact::SourceFile)
            continue;

        const auto parentRuleNodes = filterByType<RuleNode>(artifact->children);
        QBS_CHECK(std::distance(parentRuleNodes.begin(), parentRuleNodes.end()) == 1);

        QBS_CHECK(transformer);
        QBS_CHECK(transformer->rule);
        QBS_CHECK(transformer->rule->product);
        QBS_CHECK(transformer->rule->product == artifact->product.get());
        QBS_CHECK(transformer->rule->product == product.get());
        QBS_CHECK(transformer->outputs.contains(artifact));
        QBS_CHECK(contains(product->rules, std::const_pointer_cast<Rule>(transformer->rule)));
        qCDebug(lcBuildGraph)
                << "The transformer has" << transformer->outputs.size() << "outputs.";
        ArtifactSet transformerOutputChildren;
        for (const Artifact * const output : std::as_const(transformer->outputs)) {
            QBS_CHECK(output->transformer == transformer);
            transformerOutputChildren.unite(ArtifactSet::filtered(output->children));
            for (const Artifact *a : filterByType<Artifact>(output->children)) {
                for (const Artifact *other : filterByType<Artifact>(output->children)) {
                    if (other != a && other->filePath() == a->filePath()
                            && (other->artifactType != Artifact::SourceFile
                                || a->artifactType != Artifact::SourceFile
                                || other->product == a->product)) {
                        throw ErrorInfo(QStringLiteral("There is more than one artifact for "
                                "file '%1' in the child list for output '%2'.")
                                .arg(a->filePath(), output->filePath()), CodeLocation(), true);
                    }
                }
            }
        }
        if (lcBuildGraph().isDebugEnabled()) {
            qCDebug(lcBuildGraph) << "The transformer output children are:";
            for (const Artifact * const a : std::as_const(transformerOutputChildren))
                qCDebug(lcBuildGraph) << "\t" << a->fileName();
            qCDebug(lcBuildGraph) << "The transformer inputs are:";
            for (const Artifact * const a : std::as_const(transformer->inputs))
                qCDebug(lcBuildGraph) << "\t" << a->fileName();
        }
        QBS_CHECK(transformer->inputs.size() <= transformerOutputChildren.size());
        for (Artifact * const transformerInput : std::as_const(transformer->inputs))
            QBS_CHECK(transformerOutputChildren.contains(transformerInput));
        transformer->artifactsMapRequestedInPrepareScript.doSanityChecks();
        transformer->artifactsMapRequestedInCommands.doSanityChecks();
    }
}

static void doSanityChecks(const ResolvedProjectPtr &project,
                           const Set<ResolvedProductPtr> &allProducts, Set<QString> &productNames,
                           const Logger &logger)
{
    logger.qbsDebug() << "Sanity checking project '" << project->name << "'";
    for (const ResolvedProjectPtr &subProject : std::as_const(project->subProjects))
        doSanityChecks(subProject, allProducts, productNames, logger);

    for (const auto &product : project->products) {
        QBS_CHECK(product->project == project);
        QBS_CHECK(product->topLevelProject() == project->topLevelProject());
        doSanityChecksForProduct(product, allProducts, logger);
        QBS_CHECK(!productNames.contains(product->uniqueName()));
        productNames << product->uniqueName();
    }
}

void doSanityChecks(const ResolvedProjectPtr &project, const Logger &logger)
{
    if (qEnvironmentVariableIsEmpty("QBS_SANITY_CHECKS"))
        return;
    Set<QString> productNames;
    const auto allProducts = rangeTo<Set<ResolvedProductPtr>>(project->allProducts());
    doSanityChecks(project, allProducts, productNames, logger);
}

} // namespace Internal
} // namespace qbs
