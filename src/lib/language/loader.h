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

#ifndef  L2_TARGETLOADER_H
#define  L2_TARGETLOADER_H

#include "language.h"
#include <tools/settings.h>
#include <tools/codelocation.h>

#include <QCoreApplication>
#include <QScriptClass>
#include <QStringList>
#include <QVariant>

#include <set>

QT_BEGIN_NAMESPACE
class QScriptContext;
QT_END_NAMESPACE

namespace qbs {

class Function
{
public:
    QString name;
    QScriptProgram source;
};

class Binding
{
public:
    QStringList name;
    QScriptProgram valueSource;

    bool isValid() const { return !name.isEmpty(); }
    CodeLocation codeLocation() const;
};

class PropertyDeclaration
{
public:
    enum Type
    {
        UnknownType,
        Boolean,
        Paths,
        String,
        Variant,
        Verbatim
    };

    enum Flag
    {
        DefaultFlags = 0,
        ListProperty = 0x1,
        PropertyNotAvailableInConfig = 0x2     // Is this property part of a project, product or file configuration?
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    PropertyDeclaration();
    PropertyDeclaration(const QString &name, Type type, Flags flags = DefaultFlags);
    ~PropertyDeclaration();

    bool isValid() const;

    QString name;
    Type type;
    Flags flags;
    QScriptValue allowedValues;
    QString description;
    QString initialValueSource;
};

class ProjectFile;

class LanguageObject
{
public:
    LanguageObject(ProjectFile *owner);
    LanguageObject(const LanguageObject &other);
    ~LanguageObject();

    QString id;

    QStringList prototype;
    QString prototypeFileName;
    CodeLocation prototypeLocation;
    ProjectFile *file;

    QList<LanguageObject *> children;
    QHash<QStringList, Binding> bindings;
    QHash<QString, Function> functions;
    QHash<QString, PropertyDeclaration> propertyDeclarations;
};

class EvaluationObject;
class Scope;

/**
  * Represents a qbs project file.
  * Owns all the language objects.
  */
class ProjectFile
{
public:
    typedef QSharedPointer<ProjectFile> Ptr;

    ProjectFile();
    ~ProjectFile();

    JsImports jsImports;
    LanguageObject *root;
    QString fileName;

    bool isValid() const { return !root->prototype.isEmpty(); }
    bool isDestructing() const { return m_destructing; }

    void registerScope(QSharedPointer<Scope> scope) { m_scopes += scope; }
    void registerEvaluationObject(EvaluationObject *eo) { m_evaluationObjects += eo; }
    void unregisterEvaluationObject(EvaluationObject *eo) { m_evaluationObjects.removeOne(eo); }
    void registerLanguageObject(LanguageObject *eo) { m_languageObjects += eo; }
    void unregisterLanguageObject(LanguageObject *eo) { m_languageObjects.removeOne(eo); }

private:
    bool m_destructing;
    QList<QSharedPointer<Scope> > m_scopes;
    QList<EvaluationObject *> m_evaluationObjects;
    QList<LanguageObject *> m_languageObjects;
};


// evaluation objects

class ScopeChain;
class Scope;

class Property
{
public:
    explicit Property(LanguageObject *sourceObject = 0);
    explicit Property(QSharedPointer<Scope> scope);
    explicit Property(EvaluationObject *object);
    explicit Property(const QScriptValue &scriptValue);

    // ids, project. etc, JS imports and
    // where properties get resolved to
    QSharedPointer<ScopeChain> scopeChain;
    QScriptProgram valueSource;
    QList<Property> baseProperties;

    // value once it's been evaluated
    QScriptValue value;

    // if value is a scope
    QSharedPointer<Scope> scope;

    // where this property is set
    LanguageObject *sourceObject;

    bool isValid() const { return scopeChain || scope || value.isValid(); }
};

class ScopeChain : public QScriptClass
{
    Q_DISABLE_COPY(ScopeChain)
    Q_DECLARE_TR_FUNCTIONS(ScopeChain)
public:
    typedef QSharedPointer<ScopeChain> Ptr;

    ScopeChain(QScriptEngine *engine, const QSharedPointer<Scope> &root = QSharedPointer<Scope>());
    ~ScopeChain();

    ScopeChain *clone() const;
    QScriptValue value();
    QSharedPointer<Scope> first() const;
    QSharedPointer<Scope> last() const;
    // returns this
    ScopeChain *prepend(const QSharedPointer<Scope> &newTop);

    QSharedPointer<Scope> findNonEmpty(const QString &name) const;
    QSharedPointer<Scope> find(const QString &name) const;

    Property lookupProperty(const QString &name) const;

protected:
    // QScriptClass interface
    QueryFlags queryProperty(const QScriptValue &object, const QScriptString &name,
                             QueryFlags flags, uint *id);
    QScriptValue property(const QScriptValue &object, const QScriptString &name, uint id);
    void setProperty(QScriptValue &object, const QScriptString &name, uint id, const QScriptValue &m_value);

private:
    QList<QWeakPointer<Scope> > m_scopes;
    QScriptValue m_value;
};

class Scope;
typedef std::set<Scope *> ScopesCache;
typedef QSharedPointer<ScopesCache> ScopesCachePtr;

class Scope : public QScriptClass
{
    Q_DISABLE_COPY(Scope)
    Q_DECLARE_TR_FUNCTIONS(Scope)
    Scope(QScriptEngine *engine, ScopesCachePtr cache, const QString &name);

    ScopesCachePtr m_scopesCache;

public:
    typedef QSharedPointer<Scope> Ptr;

    static Ptr create(QScriptEngine *engine, ScopesCachePtr cache, const QString &name,
                      ProjectFile *owner);
    ~Scope();

    QString name() const;

protected:
    // QScriptClass interface
    QueryFlags queryProperty(const QScriptValue &object, const QScriptString &name,
                             QueryFlags flags, uint *id);
    QScriptValue property(const QScriptValue &object, const QScriptString &name, uint id);

public:
    QScriptValue property(const QString &name) const;
    bool boolValue(const QString &name, bool defaultValue = false) const;
    QString stringValue(const QString &name) const;
    QStringList stringListValue(const QString &name) const;
    QString verbatimValue(const QString &name) const;
    void dump(const QByteArray &indent) const;
    void insertAndDeclareProperty(const QString &propertyName, const Property &property,
                                  PropertyDeclaration::Type propertyType = PropertyDeclaration::Variant);

    QHash<QString, Property> properties;
    QHash<QString, PropertyDeclaration> declarations;

    QString m_name;
    QWeakPointer<Scope> fallbackScope;
    QScriptValue value;
};

class ProbeScope : public QScriptClass
{
    Q_DISABLE_COPY(ProbeScope)
    Q_DECLARE_TR_FUNCTIONS(ProbeScope)
    ProbeScope(QScriptEngine *engine, const Scope::Ptr &scope);
public:
    typedef QSharedPointer<ProbeScope> Ptr;

    static Ptr create(QScriptEngine *engine, const Scope::Ptr &scope);
    ~ProbeScope();

    QScriptValue value();

protected:
    // QScriptClass interface
    QueryFlags queryProperty(const QScriptValue &object, const QScriptString &name,
                             QueryFlags flags, uint *id);
    QScriptValue property(const QScriptValue &object, const QScriptString &name, uint id);
    void setProperty(QScriptValue &object, const QScriptString &name, uint id, const QScriptValue &value);

private:
    Scope::Ptr m_scope;
    QSharedPointer<QScriptClass> m_scopeChain;
    QScriptValue m_value;
};

class ModuleBase
{
public:
    typedef QSharedPointer<ModuleBase> Ptr;

    virtual ~ModuleBase() {}
    QString name;
    QScriptProgram condition;
    ScopeChain::Ptr conditionScopeChain;
    CodeLocation dependsLocation;
};

class UnknownModule : public ModuleBase
{
public:
    typedef QSharedPointer<UnknownModule> Ptr;

    bool required;
    QString failureMessage;
};

class EvaluationObject;

class Module : public ModuleBase
{
    Q_DISABLE_COPY(Module)
public:
    typedef QSharedPointer<Module> Ptr;

    Module();
    ~Module();

    ProjectFile *file() const;
    void dump(QByteArray &indent);

    QStringList id;
    Scope::Ptr context;
    EvaluationObject *object;
    uint dependsCount;
};

class EvaluationObject
{
    Q_DISABLE_COPY(EvaluationObject)
public:
    EvaluationObject(LanguageObject *instantiatingObject);
    ~EvaluationObject();

    LanguageObject *instantiatingObject() const;
    void dump(QByteArray &indent);

    QString prototype;

    Scope::Ptr scope;
    QList<EvaluationObject *> children;
    QHash<QString, Module::Ptr> modules;
    QList<UnknownModule::Ptr> unknownModules;

    // the source objects that generated this object
    // the object that triggered the instantiation is first, followed by prototypes
    QList<LanguageObject *> objects;
};

class ProgressObserver;
class QbsEngine;

class Loader
{
    Q_DECLARE_TR_FUNCTIONS(Loader)
public:
    Loader(QbsEngine *engine);
    ~Loader();
    void setProgressObserver(ProgressObserver *observer);
    void setSearchPaths(const QStringList &searchPaths);
    ProjectFile::Ptr loadProject(const QString &fileName);
    ResolvedProject::Ptr resolveProject(ProjectFile::Ptr projectFile, const QString &buildDirectoryRoot,
                                        const QVariantMap &userProperties, bool resolveProductDependencies = true);
    static QSet<QString> resolveFiles(const Group::ConstPtr &group, const QString &baseDir);

protected:
    static QSet<QString> resolveFiles(const Group::ConstPtr &group, const QStringList &patterns, const QString &baseDir);
    static void resolveFiles(QSet<QString> &files, const QString &baseDir, bool recursive,
                             const QStringList &parts, int index = 0);

    ProjectFile::Ptr parseFile(const QString &fileName);

    void clearScopesCache();
    Scope::Ptr buildFileContext(ProjectFile *file);
    bool existsModuleInSearchPath(const QString &moduleName);
    Module::Ptr searchAndLoadModule(const QStringList &moduleId, const QString &moduleName, ScopeChain::Ptr moduleScope,
                                    const QVariantMap &userProperties, const CodeLocation &dependsLocation,
                                    const QStringList &extraSearchPaths = QStringList());
    Module::Ptr loadModule(ProjectFile *file, const QStringList &moduleId, const QString &moduleName, ScopeChain::Ptr moduleBaseScope,
                           const QVariantMap &userProperties, const CodeLocation &dependsLocation);
    void insertModulePropertyIntoScope(Scope::Ptr targetScope, const Module::Ptr &module, Scope::Ptr moduleInstance = Scope::Ptr());
    QList<Module::Ptr> evaluateDependency(LanguageObject *depends, ScopeChain::Ptr conditionScopeChain,
                                          ScopeChain::Ptr moduleScope, const QStringList &extraSearchPaths,
                                          QList<UnknownModule::Ptr> *unknownModules, const QVariantMap &userProperties);
    void evaluateDependencies(LanguageObject *object, EvaluationObject *evaluationObject, const ScopeChain::Ptr &localScope,
                              ScopeChain::Ptr moduleScope, const QVariantMap &userProperties, bool loadBaseModule = true);
    void evaluateDependencyConditions(EvaluationObject *evaluationObject);
    void evaluateImports(Scope::Ptr target, const JsImports &jsImports);
    void evaluatePropertyOptions(LanguageObject *object);
    void resolveInheritance(LanguageObject *object, EvaluationObject *evaluationObject,
                            ScopeChain::Ptr moduleScope = ScopeChain::Ptr(), const QVariantMap &userProperties = QVariantMap());
    void fillEvaluationObject(const ScopeChain::Ptr &scope, LanguageObject *object, Scope::Ptr ids, EvaluationObject *evaluationObject, const QVariantMap &userProperties);
    void fillEvaluationObjectBasics(const ScopeChain::Ptr &scope, LanguageObject *object, EvaluationObject *evaluationObject);
    void setupInternalPrototype(LanguageObject *object, EvaluationObject *evaluationObject);
    void resolveModule(ResolvedProduct::Ptr rproduct, const QString &moduleName, EvaluationObject *module);
    void resolveGroup(ResolvedProduct::Ptr rproduct, EvaluationObject *product, EvaluationObject *group);
    void resolveProductModule(const ResolvedProduct::ConstPtr &rproduct, EvaluationObject *group);
    void resolveTransformer(ResolvedProduct::Ptr rproduct, EvaluationObject *trafo, ResolvedModule::ConstPtr module);
    void resolveProbe(EvaluationObject *node);
    QList<EvaluationObject *> resolveCommonItems(const QList<EvaluationObject *> &objects,
                                                    ResolvedProduct::Ptr rproduct, const ResolvedModule::ConstPtr &module);
    Rule::Ptr resolveRule(EvaluationObject *object, ResolvedModule::ConstPtr module);
    FileTagger::ConstPtr resolveFileTagger(EvaluationObject *evaluationObject);
    void buildModulesProperty(EvaluationObject *evaluationObject);
    void checkModuleDependencies(const Module::Ptr &module);

    class ProductData
    {
    public:
        QString originalProductName;
        EvaluationObject *product;
        QList<UnknownModule::Ptr> usedProducts;
        QList<UnknownModule::Ptr> usedProductsFromProductModule;

        void addUsedProducts(const QList<UnknownModule::Ptr> &additionalUsedProducts, bool *productsAdded);
    };

    struct ProjectData
    {
        QHash<ResolvedProduct::Ptr, ProductData> products;
        QList<ProductData> removedProducts;
    };

    void resolveTopLevel(const ResolvedProject::Ptr &rproject,
                         LanguageObject *object,
                         const QString &projectFileName,
                         ProjectData *projectData,
                         QList<Rule::Ptr> *globalRules,
                         QList<FileTagger::ConstPtr> *globalFileTaggers,
                         const QVariantMap &userProperties,
                         const ScopeChain::Ptr &scope,
                         const ResolvedModule::ConstPtr &dummyModule);

private:
    static Loader *get(QScriptEngine *engine);
    static QScriptValue js_getHostOS(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_getHostDefaultArchitecture(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_getenv(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_configurationValue(QScriptContext *context, QScriptEngine *engine);

    static QHash<QString, PropertyDeclaration> m_dependsPropertyDeclarations;
    static QHash<QString, PropertyDeclaration> m_groupPropertyDeclarations;

    ProgressObserver *m_progressObserver;
    QStringList m_searchPaths;
    QStringList m_moduleSearchPaths;
    QbsEngine *m_engine;
    ScopesCachePtr m_scopesWithEvaluatedProperties;
    QScriptValue m_jsFunction_getHostOS;
    QScriptValue m_jsFunction_getHostDefaultArchitecture;
    QScriptValue m_jsFunction_getenv;
    QScriptValue m_jsFunction_configurationValue;
    QScriptValue m_probeScriptScope;
    Settings::Ptr m_settings;
    ProjectFile::Ptr m_project;
    QHash<QString, ProjectFile::Ptr> m_parsedFiles;
    QHash<QString, QScriptValue> m_jsImports;   // ### remove
    QHash<QString, QVariantMap> m_productModules;
    QHash<QString, QStringList> m_moduleDirListCache;
};

} // namespace qbs

#endif
