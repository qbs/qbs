/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#ifndef  L2_TARGETLOADER_H
#define  L2_TARGETLOADER_H

#include "language.h"
#include <tools/settings.h>
#include <tools/codelocation.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#if QT_VERSION >= 0x050000
#    include <QtConcurrent/QFutureInterface>
#else
#    include <QtCore/QFutureInterface>
#endif
#include <QtScript/QScriptClass>
#include <QtScript/QScriptEngine>

#include <set>



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

    QString name;
    Type type;
    Flags flags;
    QVariant allowedValues;
    QString description;
    // the initial value will be in the bindings list
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
    qbs::CodeLocation prototypeLocation;
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
    Property() {}
    explicit Property(QSharedPointer<Scope> scope)
        : scope(scope)
    {}
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

    bool isValid() const { return scopeChain || scope || value.isValid(); }
};

class ScopeChain : public QScriptClass
{
    Q_DISABLE_COPY(ScopeChain)
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
    QScriptValue m_globalObject;
};

class Scope : public QScriptClass
{
    Q_DISABLE_COPY(Scope)
    Scope(QScriptEngine *engine, const QString &name);
public:
    typedef QSharedPointer<Scope> Ptr;
    static std::set<Scope *> scopesWithEvaluatedProperties;

    static Ptr create(QScriptEngine *engine, const QString &name, ProjectFile *owner);
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

    QHash<QString, Property> properties;
    QHash<QString, PropertyDeclaration> declarations;

    QString m_name;
    QWeakPointer<Scope> fallbackScope;
    QScriptValue value;
};

class Module;

class UnknownModule
{
public:
    QString name;
    bool required;
    QString failureMessage;
    qbs::CodeLocation dependsLocation;
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
    QHash<QString, QSharedPointer<Module> > modules;
    QList<UnknownModule> unknownModules;

    // the source objects that generated this object
    // the object that triggered the instantiation is first, followed by prototypes
    QList<LanguageObject *> objects;
};

class Module
{
    Q_DISABLE_COPY(Module)
public:
    typedef QSharedPointer<Module> Ptr;

    Module();
    ~Module();

    ProjectFile *file() const;
    void dump(QByteArray &indent);

    QString id;
    QString name;
    qbs::CodeLocation dependsLocation;

    Scope::Ptr context;
    EvaluationObject *object;
};

class Loader
{
    Q_DECLARE_TR_FUNCTIONS(Loader)
public:
    Loader();
    ~Loader();
    void setSearchPaths(const QStringList &searchPaths);
    ProjectFile::Ptr loadProject(const QString &fileName);
    bool hasLoaded() const { return m_project; }
    int productCount(Configuration::Ptr userProperties);
    ResolvedProject::Ptr resolveProject(const QString &buildDirectoryRoot,
                                        Configuration::Ptr userProperties,
                                        QFutureInterface<bool> &futureInterface,
                                        bool resolveProductDependencies = true);

protected:
    ProjectFile::Ptr parseFile(const QString &fileName);

    Scope::Ptr buildFileContext(ProjectFile *file);
    Module::Ptr loadModule(const QString &moduleId, const QString &moduleName, ScopeChain::Ptr moduleScope,
                           const QVariantMap &userProperties, const qbs::CodeLocation &dependsLocation,
                           const QStringList &extraSearchPaths = QStringList());
    Module::Ptr loadModule(ProjectFile *file, const QString &moduleId, const QString &moduleName, ScopeChain::Ptr moduleBaseScope,
                           const QVariantMap &userProperties, const qbs::CodeLocation &dependsLocation);
    QList<Module::Ptr> evaluateDependency(EvaluationObject *parentEObj, LanguageObject *depends,
                                          ScopeChain::Ptr moduleScope,
                                          const QStringList &extraSearchPaths, QList<UnknownModule> *unknownModules, const QVariantMap &userProperties);
    void evaluateDependencies(LanguageObject *object, EvaluationObject *evaluationObject, const ScopeChain::Ptr &localScope,
                              ScopeChain::Ptr moduleScope, const QVariantMap &userProperties, bool loadBaseModule = true);
    void evaluateImports(Scope::Ptr target, const JsImports &jsImports);
    void evaluatePropertyOptions(LanguageObject *object);
    void resolveInheritance(LanguageObject *object, EvaluationObject *evaluationObject,
                            ScopeChain::Ptr moduleScope = ScopeChain::Ptr(), const QVariantMap &userProperties = QVariantMap());
    void fillEvaluationObject(const ScopeChain::Ptr &scope, LanguageObject *object, Scope::Ptr ids, EvaluationObject *evaluationObject, const QVariantMap &userProperties);
    void fillEvaluationObjectBasics(const ScopeChain::Ptr &scope, LanguageObject *object, EvaluationObject *evaluationObject);
    void fillEvaluationObjectForProperties(const ScopeChain::Ptr &scope, LanguageObject *object, Scope::Ptr ids, EvaluationObject *evaluationObject, const QVariantMap &userProperties);
    void setupInternalPrototype(EvaluationObject *evaluationObject);
    void resolveModule(ResolvedProduct::Ptr rproduct, const QString &moduleName, EvaluationObject *module);
    void resolveGroup(ResolvedProduct::Ptr rproduct, EvaluationObject *product, EvaluationObject *group);
    void resolveProductModule(ResolvedProduct::Ptr rproduct, EvaluationObject *product, EvaluationObject *group);
    void resolveTransformer(ResolvedProduct::Ptr rproduct, EvaluationObject *trafo, ResolvedModule::Ptr module);
    QList<EvaluationObject *> resolveCommonItems(const QList<EvaluationObject *> &objects,
                                                    ResolvedProduct::Ptr rproduct, ResolvedModule::Ptr module);
    Rule::Ptr resolveRule(EvaluationObject *object, ResolvedModule::Ptr module);
    void buildModulesProperty(EvaluationObject *evaluationObject);

    class ProductData
    {
    public:
        EvaluationObject *product;
        QList<UnknownModule> usedProducts;
        QList<UnknownModule> usedProductsFromProductModule;

        void addUsedProducts(const QList<UnknownModule> &additionalUsedProducts, bool *productsAdded);
    };
    typedef QHash<ResolvedProduct::Ptr, ProductData> ProjectData;

    EvaluationObject *resolveTopLevel(const ResolvedProject::Ptr &rproject,
                                      LanguageObject *object,
                                      const QString &projectFileName,
                                      ProjectData *projectData,
                                      QList<Rule::Ptr> *globalRules,
                                      const Configuration::Ptr &userProperties,
                                      const ScopeChain::Ptr &scope,
                                      const ResolvedModule::Ptr &dummyModule,
                                      QFutureInterface<bool> &futureInterface);

private:
    static Loader *get(QScriptEngine *engine);
    static QScriptValue js_getHostOS(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_getHostDefaultArchitecture(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_configurationValue(QScriptContext *context, QScriptEngine *engine);

    static QHash<QString, PropertyDeclaration> m_dependsPropertyDeclarations;

    QStringList m_searchPaths;
    QScriptEngine m_engine;
    QScriptValue m_jsFunction_getHostOS;
    QScriptValue m_jsFunction_getHostDefaultArchitecture;
    QScriptValue m_jsFunction_configurationValue;
    Settings::Ptr m_settings;
    ProjectFile::Ptr m_project;
    QHash<QString, ProjectFile::Ptr> m_parsedFiles;
    QHash<QString, QScriptValue> m_jsImports;
    QHash<Rule::Ptr, EvaluationObject *> m_ruleMap;
    QHash<QString, QVariantMap> m_productModules;
};

} // namespace qbs

#endif
