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

#undef QT_NO_CAST_FROM_ASCII // I am qmake, and I approve this hack.

#include "tst_language.h"

#include <language/evaluator.h>
#include <language/filecontext.h>
#include <language/identifiersearch.h>
#include <language/item.h>
#include <language/itempool.h>
#include <language/language.h>
#include <language/propertymapinternal.h>
#include <language/scriptengine.h>
#include <language/value.h>
#include <parser/qmljslexer_p.h>
#include <parser/qmljsparser_p.h>
#include <tools/scripttools.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/propertyfinder.h>
#include <tools/settings.h>

#include <QProcessEnvironment>

#include <algorithm>

Q_DECLARE_METATYPE(QList<bool>)

namespace qbs {
namespace Internal {
static QString testDataDir() { return FileInfo::resolvePath(QLatin1String(SRCDIR),
                                                            QLatin1String("language/testdata")); }
static QString testProject(const char *fileName) {
    return testDataDir() + QLatin1Char('/') + QLatin1String(fileName);
}

TestLanguage::TestLanguage(ILogSink *logSink)
    : m_logSink(logSink)
    , m_wildcardsTestDirPath(QDir::tempPath() + QLatin1String("/_wildcards_test_dir_"))
{
    qsrand(QTime::currentTime().msec());
    qRegisterMetaType<QList<bool> >("QList<bool>");
    defaultParameters.setBuildRoot("/some/build/directory");
    defaultParameters.setPropertyCheckingMode(SetupProjectParameters::PropertyCheckingStrict);
}

TestLanguage::~TestLanguage()
{
}

QHash<QString, ResolvedProductPtr> TestLanguage::productsFromProject(ResolvedProjectPtr project)
{
    QHash<QString, ResolvedProductPtr> result;
    foreach (const ResolvedProductPtr &product, project->allProducts())
        result.insert(product->name, product);
    return result;
}

ResolvedModuleConstPtr TestLanguage::findModuleByName(ResolvedProductPtr product, const QString &name)
{
    foreach (const ResolvedModuleConstPtr &module, product->modules)
        if (module->name == name)
            return module;
    return ResolvedModuleConstPtr();
}

QVariant TestLanguage::productPropertyValue(ResolvedProductPtr product, QString propertyName)
{
    QStringList propertyNameComponents = propertyName.split(QLatin1Char('.'));
    QVariantMap properties;
    if (propertyNameComponents.count() > 1) {
        propertyNameComponents.prepend(QLatin1String("modules"));
        properties = product->moduleProperties->value();
    } else {
        properties = product->productProperties;
    }
    return getConfigProperty(properties, propertyNameComponents);
}

void TestLanguage::handleInitCleanupDataTags(const char *projectFileName, bool *handled)
{
    const QByteArray dataTag = QTest::currentDataTag();
    if (dataTag == "init") {
        *handled = true;
        bool exceptionCaught = false;
        try {
            defaultParameters.setProjectFilePath(testProject(projectFileName));
            project = loader->loadProject(defaultParameters);
            QVERIFY(project);
        } catch (const ErrorInfo &e) {
            exceptionCaught = true;
            qDebug() << e.toString();
        }
        QCOMPARE(exceptionCaught, false);
    } else if (dataTag == "cleanup") {
        *handled = true;
        project.clear();
    } else {
        *handled = false;
    }
}

#define HANDLE_INIT_CLEANUP_DATATAGS(fn) {\
    bool handled;\
    handleInitCleanupDataTags(fn, &handled);\
    if (handled)\
        return;\
    QVERIFY(project);\
}

void TestLanguage::initTestCase()
{
    m_logger = Logger(m_logSink);
    m_engine = new ScriptEngine(m_logger, this);
    loader = new Loader(m_engine, m_logger);
    loader->setSearchPaths(QStringList()
                           << QLatin1String(SRCDIR "/../../../share/qbs"));
    defaultParameters.setTopLevelProfile("qbs_autotests");
    defaultParameters.setBuildVariant("debug");
    defaultParameters.expandBuildConfiguration();
    QVERIFY(QFileInfo(m_wildcardsTestDirPath).isAbsolute());
}

void TestLanguage::cleanupTestCase()
{
    delete loader;
}

void TestLanguage::baseProperty()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("baseproperty.qbs"));
        project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        ResolvedProductPtr product = products.value("product1");
        QVERIFY(product);
        QVariantMap cfg = product->productProperties;
        QCOMPARE(cfg.value("narf").toStringList(), QStringList() << "boo");
        QCOMPARE(cfg.value("zort").toStringList(), QStringList() << "bar" << "boo");
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::buildConfigStringListSyntax()
{
    bool exceptionCaught = false;
    try {
        SetupProjectParameters parameters = defaultParameters;
        QVariantMap overriddenValues;
        overriddenValues.insert("project.someStrings", "foo,bar,baz");
        parameters.setOverriddenValues(overriddenValues);
        parameters.setProjectFilePath(testProject("buildconfigstringlistsyntax.qbs"));
        project = loader->loadProject(parameters);
        QVERIFY(project);
        QCOMPARE(project->projectProperties().value("someStrings").toStringList(),
                 QStringList() << "foo" << "bar" << "baz");
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::builtinFunctionInSearchPathsProperty()
{
    bool exceptionCaught = false;
    try {
        SetupProjectParameters parameters = defaultParameters;
        parameters.setProjectFilePath(testProject("builtinFunctionInSearchPathsProperty.qbs"));
        QVERIFY(loader->loadProject(parameters));
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::canonicalArchitecture()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("canonicalArchitecture.qbs"));
        project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        ResolvedProductPtr product = products.value(QLatin1String("x86"));
        QVERIFY(product);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::rfc1034Identifier()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("rfc1034identifier.qbs"));
        project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        ResolvedProductPtr product = products.value(QLatin1String("this-has-special-characters-"
                                                                  "uh-oh-Undersc0r3s-Are.Bad"));
        QVERIFY(product);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::conditionalDepends()
{
    bool exceptionCaught = false;
    ResolvedProductPtr product;
    ResolvedModuleConstPtr dependency;
    try {
        defaultParameters.setProjectFilePath(testProject("conditionaldepends.qbs"));
        project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);

        product = products.value("conditionaldepends_derived");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(dependency);

        product = products.value("conditionaldepends_derived_false");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy");
        QCOMPARE(dependency, ResolvedModuleConstPtr());

        product = products.value("product_props_true");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(dependency);

        product = products.value("product_props_false");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy");
        QCOMPARE(dependency, ResolvedModuleConstPtr());

        product = products.value("project_props_true");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(dependency);

        product = products.value("project_props_false");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy");
        QCOMPARE(dependency, ResolvedModuleConstPtr());

        product = products.value("module_props_true");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy2");
        QVERIFY(dependency);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(dependency);

        product = products.value("module_props_false");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy2");
        QVERIFY(dependency);
        dependency = findModuleByName(product, "dummy");
        QCOMPARE(dependency, ResolvedModuleConstPtr());

        product = products.value("contradictory_conditions1");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(dependency);

        product = products.value("contradictory_conditions2");
        QVERIFY(product);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(dependency);

        product = products.value("unknown_dependency_condition_false");
        QVERIFY(product);
        dependency = findModuleByName(product, "doesonlyexistifhellfreezesover");
        QVERIFY(!dependency);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void qbs::Internal::TestLanguage::dependencyOnAllProfiles()
{
    bool exceptionCaught = false;
    try {
        SetupProjectParameters params = defaultParameters;
        params.setProjectFilePath(testProject("dependencyOnAllProfiles.qbs"));
        Settings settings((QString()));
        TemporaryProfile p1("p1", &settings);
        p1.p.setValue("qbs.architecture", "arch1");
        TemporaryProfile p2("p2", &settings);
        p2.p.setValue("qbs.architecture", "arch2");
        QVariantMap overriddenValues;
        overriddenValues.insert("project.profile1", "p1");
        overriddenValues.insert("project.profile2", "p2");
        params.setOverriddenValues(overriddenValues);
        project = loader->loadProject(params);
        QVERIFY(project);
        QCOMPARE(project->products.count(), 3);
        const ResolvedProductConstPtr mainProduct = productsFromProject(project).value("main");
        QVERIFY(mainProduct);
        QCOMPARE(mainProduct->dependencies.count(), 2);
        foreach (const ResolvedProductConstPtr &p, mainProduct->dependencies) {
            QCOMPARE(p->name, QLatin1String("dep"));
            QVERIFY(p->profile == "p1" || p->profile == "p2");
        }
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::defaultValue()
{
    bool exceptionCaught = false;
    try {
        SetupProjectParameters params = defaultParameters;
        params.setProjectFilePath(testProject("defaultvalue/egon.qbs"));
        QFETCH(QString, prop1Value);
        QVariantMap overridden;
        if (!prop1Value.isEmpty())
            overridden.insert("lower.prop1", prop1Value);
        params.setOverriddenValues(overridden);
        TopLevelProjectPtr project = loader->loadProject(params);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 2);
        const ResolvedProductPtr product = products.value("egon");
        QVERIFY(product);
        QStringList propertyName = QStringList() << "modules" << "lower" << "prop2";
        QVariant propertyValue = getConfigProperty(product->moduleProperties->value(), propertyName);
        QFETCH(QVariant, expectedProp2Value);
        QCOMPARE(propertyValue, expectedProp2Value);
        propertyName = QStringList() << "modules" << "lower" << "listProp";
        propertyValue = getConfigProperty(product->moduleProperties->value(), propertyName);
        QFETCH(QVariant, expectedListPropValue);
        QEXPECT_FAIL("controlling property not overwritten", "QBS-845", Continue);
        QCOMPARE(propertyValue.toStringList(), expectedListPropValue.toStringList());
    }
    catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::defaultValue_data()
{
    QTest::addColumn<QString>("prop1Value");
    QTest::addColumn<QVariant>("expectedProp2Value");
    QTest::addColumn<QVariant>("expectedListPropValue");
    QTest::newRow("controlling property with random value") << "random" << QVariant("withoutBlubb")
            << QVariant(QStringList({"other", "other"}));
    QTest::newRow("controlling property with blubb value") << "blubb" << QVariant("withBlubb")
            << QVariant(QStringList({"blubb", "other", "blubb", "other"}));
    QTest::newRow("controlling property with egon value") << "egon" << QVariant("withEgon")
            << QVariant(QStringList({"egon", "other"}));
    QTest::newRow("controlling property not overwritten") << "" << QVariant("withBlubb")
            << QVariant(QStringList({"blubb", "other", "blubb", "other"}));
}

void TestLanguage::environmentVariable()
{
    bool exceptionCaught = false;
    try {
        // Create new environment:
        const QString varName = QLatin1String("PRODUCT_NAME");
        const QString productName = QLatin1String("MyApp") + QString::number(qrand());
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(varName, productName);

        QProcessEnvironment origEnv = defaultParameters.environment(); // store orig environment

        defaultParameters.setEnvironment(env);
        defaultParameters.setProjectFilePath(testProject("environmentvariable.qbs"));
        project = loader->loadProject(defaultParameters);

        defaultParameters.setEnvironment(origEnv); // reset environment

        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        ResolvedProductPtr product = products.value(productName);
        QVERIFY(product);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::erroneousFiles_data()
{
    QTest::addColumn<QString>("errorMessage");
    QTest::newRow("unknown_module")
            << "Product dependency 'neitherModuleNorProduct' not found";
    QTest::newRow("multiple_exports")
            << "Multiple Export items in one product are prohibited.";
    QTest::newRow("multiple_properties_in_subproject")
            << "Multiple instances of item 'Properties' found where at most one "
               "is allowed.";
    QTest::newRow("importloop1")
            << "Loop detected when importing";
    QTest::newRow("nonexistentouter")
            << "Can't find variable: outer";
    QTest::newRow("invalid_file")
            << "does not exist";
    QTest::newRow("invalid_property_type")
            << "Unknown type 'nonsense' in property declaration.";
    QTest::newRow("reserved_name_in_import")
            << "Cannot reuse the name of built-in extension 'TextFile'.";
    QTest::newRow("throw_in_property_binding")
            << "something is wrong";
    QTest::newRow("dependency_cycle")
            << "Cyclic dependencies detected.";
    QTest::newRow("references_cycle")
            << "Cycle detected while referencing file 'references_cycle.qbs'.";
    QTest::newRow("subproject_cycle")
            << "Cycle detected while loading subproject file 'subproject_cycle.qbs'.";
    QTest::newRow("invalid_stringlist_element")
            << "Expected array element of type String at index 1.";
    QTest::newRow("undefined_stringlist_element")
            << "Array element at index 1 is undefined. String expected.";
    QTest::newRow("undeclared_item")
            << "Item 'cpp' is not declared.";
    QTest::newRow("undeclared_property_wrapper")
            << "Property 'doesntexist' is not declared.";
    QTest::newRow("undeclared_property_in_export_item")
            << "Property 'blubb' is not declared.";
    QTest::newRow("undeclared_property_in_export_item2")
            << "Item 'something' is not declared.";
    QTest::newRow("undeclared_property_in_export_item3")
            << "Property 'blubb' is not declared.";
    QTest::newRow("unknown_item_type")
            << "Unexpected item type 'Narf'";
    QTest::newRow("invalid_child_item_type")
            << "Items of type 'Project' cannot contain items of type 'Depends'.";
    QTest::newRow("conflicting_fileTagsFilter")
            << "Conflicting fileTagsFilter in Group items";
    QTest::newRow("duplicate_sources")
            << "Duplicate source file '.*main.cpp' "
               "at .*duplicate_sources.qbs:4:12 and .*duplicate_sources.qbs:6:16.";
    QTest::newRow("duplicate_sources_wildcards")
            << "Duplicate source file '.*duplicate_sources_wildcards.qbs' "
               "at .*duplicate_sources_wildcards.qbs:4:12 "
               "and .*duplicate_sources_wildcards.qbs:6:16.";
    QTest::newRow("oldQbsVersion")
            << "The project requires at least qbs version \\d+\\.\\d+.\\d+, "
               "but this is qbs version " QBS_VERSION ".";
    QTest::newRow("wrongQbsVersionFormat")
            << "The value '.*' of Project.minimumQbsVersion is not a valid version string.";
    QTest::newRow("properties-item-with-invalid-condition")
            << "TypeError: Result of expression 'cpp.nonexistingproperty'";
    QTest::newRow("misused-inherited-property") << "Binding to non-item property";
    QTest::newRow("undeclared_property_in_Properties_item") << "Item 'blubb' is not declared";
}

void TestLanguage::erroneousFiles()
{
    QFETCH(QString, errorMessage);
    QString fileName = QString::fromLocal8Bit(QTest::currentDataTag()) + QLatin1String(".qbs");
    try {
        defaultParameters.setProjectFilePath(testProject("/erroneous/") + fileName);
        loader->loadProject(defaultParameters);
    } catch (const ErrorInfo &e) {
        if (!e.toString().contains(QRegExp(errorMessage))) {
            qDebug() << "Message:  " << e.toString();
            qDebug() << "Expected: " << errorMessage;
            QFAIL("Unexpected error message.");
        }
        return;
    }
    QEXPECT_FAIL("misused-inherited-property", "QBS-847", Continue);
    QEXPECT_FAIL("undeclared_property_in_Properties_item", "Too expensive to check", Continue);
    QVERIFY(!"No error thrown on invalid input.");
}

void TestLanguage::exports()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("exports.qbs"));
        TopLevelProjectPtr project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 12);
        ResolvedProductPtr product;
        product = products.value("myapp");
        QVERIFY(product);
        QStringList propertyName = QStringList() << "modules" << "dummy" << "defines";
        QVariant propertyValue = getConfigProperty(product->moduleProperties->value(), propertyName);
        QCOMPARE(propertyValue.toStringList(), QStringList() << "BUILD_MYAPP" << "USE_MYLIB"
                 << "USE_MYLIB2");
        propertyName = QStringList() << "modules" << "dummy" << "includePaths";
        QVariantList propertyValues = getConfigProperty(product->moduleProperties->value(),
                                                        propertyName).toList();
        QCOMPARE(propertyValues.count(), 3);
        QVERIFY(propertyValues.at(0).toString().endsWith("/app"));
        QVERIFY(propertyValues.at(1).toString().endsWith("/subdir/lib"));
        QVERIFY(propertyValues.at(2).toString().endsWith("/subdir2/lib"));

        QCOMPARE(PropertyFinder().propertyValue(product->moduleProperties->value(), "dummy",
                                                "productName").toString(), QString("myapp"));

        product = products.value("mylib");
        QVERIFY(product);
        propertyName = QStringList() << "modules" << "dummy" << "defines";
        propertyValue = getConfigProperty(product->moduleProperties->value(), propertyName);
        QCOMPARE(propertyValue.toStringList(), QStringList() << "BUILD_MYLIB");

        product = products.value("mylib2");
        QVERIFY(product);
        propertyName = QStringList() << "modules" << "dummy" << "defines";
        propertyValue = getConfigProperty(product->moduleProperties->value(), propertyName);
        QCOMPARE(propertyValue.toStringList(), QStringList() << "BUILD_MYLIB2");

        product = products.value("A");
        QVERIFY(product);
        QVERIFY(product->dependencies.contains(products.value("B")));
        QVERIFY(product->dependencies.contains(products.value("C")));
        QVERIFY(product->dependencies.contains(products.value("D")));
        product = products.value("B");
        QVERIFY(product);
        QVERIFY(product->dependencies.isEmpty());
        product = products.value("C");
        QVERIFY(product);
        QVERIFY(product->dependencies.isEmpty());
        product = products.value("D");
        QVERIFY(product);
        QVERIFY(product->dependencies.isEmpty());

        product = products.value("myapp2");
        QVERIFY(product);
        propertyName = QStringList() << "modules" << "dummy" << "cxxFlags";
        propertyValue = getConfigProperty(product->moduleProperties->value(), propertyName);
        QCOMPARE(propertyValue.toStringList(), QStringList() << "-bar");
        propertyName = QStringList() << "modules" << "dummy" << "defines";
        propertyValue = getConfigProperty(product->moduleProperties->value(), propertyName);
        QCOMPARE(propertyValue.toStringList(), QStringList() << "ABC");
        QCOMPARE(PropertyFinder().propertyValue(product->moduleProperties->value(), "dummy",
                                                "productName").toString(), QString("myapp2"));
        QCOMPARE(PropertyFinder().propertyValue(product->moduleProperties->value(), "dummy",
                "upperCaseProductName").toString(), QString("MYAPP2"));

        // Check whether we're returning incorrect cached values.
        product = products.value("myapp3");
        QVERIFY(product);
        QCOMPARE(PropertyFinder().propertyValue(product->moduleProperties->value(), "dummy",
                                                "productName").toString(), QString("myapp3"));
        QCOMPARE(PropertyFinder().propertyValue(product->moduleProperties->value(), "dummy",
                "upperCaseProductName").toString(), QString("MYAPP3"));

        // Verify we refer to the right "project" variable.
        product = products.value("sub p2");
        QVERIFY(product);
        QCOMPARE(PropertyFinder().propertyValue(product->moduleProperties->value(), "dummy",
                                                "someString").toString(), QString("sub1"));
    }
    catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::fileContextProperties()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("filecontextproperties.qbs"));
        project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        ResolvedProductPtr product = products.value("product1");
        QVERIFY(product);
        QVariantMap cfg = product->productProperties;
        QCOMPARE(cfg.value("narf").toString(), defaultParameters.projectFilePath());
        QString dirPath = QFileInfo(defaultParameters.projectFilePath()).absolutePath();
        QCOMPARE(cfg.value("zort").toString(), dirPath);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::getNativeSetting()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("getNativeSetting.qbs"));
        project = loader->loadProject(defaultParameters);

        QString expectedProductName;
        if (HostOsInfo::isOsxHost())
            expectedProductName = QLatin1String("Mac OS X");
        else if (HostOsInfo::isWindowsHost())
            expectedProductName = QLatin1String("Windows");
        else
            expectedProductName = QLatin1String("Unix");

        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        ResolvedProductPtr product = products.value(expectedProductName);
        QVERIFY(product);
        ResolvedProductPtr product2 = products.value(QLatin1String("fallback"));
        QVERIFY(product2);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::groupConditions_data()
{
    QTest::addColumn<int>("groupCount");
    QTest::addColumn<QList<bool> >("groupEnabled");
    QTest::newRow("init") << 0 << QList<bool>();
    QTest::newRow("no_condition_no_group")
            << 1 << (QList<bool>() << true);
    QTest::newRow("no_condition")
            << 2 << (QList<bool>() << true << true);
    QTest::newRow("true_condition")
            << 2 << (QList<bool>() << true << true);
    QTest::newRow("false_condition")
            << 2 << (QList<bool>() << true << false);
    QTest::newRow("true_condition_from_product")
            << 2 << (QList<bool>() << true << true);
    QTest::newRow("true_condition_from_project")
            << 2 << (QList<bool>() << true << true);
    QTest::newRow("condition_accessing_module_property")
            << 2 << (QList<bool>() << true << false);
    QTest::newRow("cleanup") << 0 << QList<bool>();
}

void TestLanguage::groupConditions()
{
    HANDLE_INIT_CLEANUP_DATATAGS("groupconditions.qbs");
    QFETCH(int, groupCount);
    QFETCH(QList<bool>, groupEnabled);
    QCOMPARE(groupCount, groupEnabled.count());
    const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
    const QString productName = QString::fromLocal8Bit(QTest::currentDataTag());
    ResolvedProductPtr product = products.value(productName);
    QVERIFY(product);
    QCOMPARE(product->name, productName);
    QCOMPARE(product->groups.count(), groupCount);
    for (int i = 0; i < groupCount; ++i) {
        if (product->groups.at(i)->enabled != groupEnabled.at(i)) {
            QFAIL(qPrintable(
                      QString("groups.at(%1)->enabled != %2").arg(i).arg(groupEnabled.at(i))));
        }
    }
}

void TestLanguage::groupName()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("groupname.qbs"));
        TopLevelProjectPtr project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 2);

        ResolvedProductPtr product = products.value("MyProduct");
        QVERIFY(product);
        QCOMPARE(product->groups.count(), 2);
        GroupConstPtr group = product->groups.at(0);
        QVERIFY(group);
        QCOMPARE(group->name, QString("MyProduct"));
        group = product->groups.at(1);
        QVERIFY(group);
        QCOMPARE(group->name, QString("MyProduct.MyGroup"));

        product = products.value("My2ndProduct");
        QVERIFY(product);
        QCOMPARE(product->groups.count(), 3);
        group = product->groups.at(0);
        QVERIFY(group);
        QCOMPARE(group->name, QString("My2ndProduct"));
        group = product->groups.at(1);
        QVERIFY(group);
        QCOMPARE(group->name, QString("My2ndProduct.MyGroup"));
        group = product->groups.at(2);
        QVERIFY(group);
        QCOMPARE(group->name, QString("Group 2"));
    }
    catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::homeDirectory()
{
    try {
        defaultParameters.setProjectFilePath(testProject("homeDirectory.qbs"));
        ResolvedProjectPtr project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 1);

        ResolvedProductPtr product = products.value("home");
        QVERIFY(product);

        QDir dir = QDir::home();
        QCOMPARE(product->productProperties.value("home").toString(), dir.canonicalPath());
        QCOMPARE(product->productProperties.value("homeSlash").toString(),
                 dir.canonicalPath());

        dir.cdUp();
        QCOMPARE(product->productProperties.value("homeUp").toString(),
                 dir.canonicalPath());

        dir = QDir::home();
        QCOMPARE(product->productProperties.value("homeFile").toString(),
                 dir.filePath("a"));

        QCOMPARE(product->productProperties.value("bogus1").toString(),
                 FileInfo::resolvePath(product->sourceDirectory, QLatin1String("a~b")));
        QCOMPARE(product->productProperties.value("bogus2").toString(),
                 FileInfo::resolvePath(product->sourceDirectory, QLatin1String("a/~/bb")));
        QCOMPARE(product->productProperties.value("user").toString(),
                 FileInfo::resolvePath(product->sourceDirectory, QLatin1String("~foo/bar")));
    }
    catch (const ErrorInfo &e) {
        qDebug() << e.toString();
    }
}

void TestLanguage::identifierSearch_data()
{
    QTest::addColumn<bool>("expectedHasNarf");
    QTest::addColumn<bool>("expectedHasZort");
    QTest::addColumn<QString>("sourceCode");
    QTest::newRow("no narf, no zort") << false << false << QString(
                                  "Product {\n"
                                  "    name: {\n"
                                  "        var foo = 'bar';\n"
                                  "        print(foo);\n"
                                  "        return foo;\n"
                                  "    }\n"
                                  "}\n");
    QTest::newRow("narf, no zort") << true << false << QString(
                                  "Product {\n"
                                  "    name: {\n"
                                  "        var foo = 'zort';\n"
                                  "        print(narf + foo);\n"
                                  "        return foo;\n"
                                  "    }\n"
                                  "}\n");
    QTest::newRow("no narf, zort") << false << true << QString(
                                  "Product {\n"
                                  "    name: {\n"
                                  "        var foo = 'narf';\n"
                                  "        print(zort + foo);\n"
                                  "        return foo;\n"
                                  "    }\n"
                                  "}\n");
    QTest::newRow("narf, zort") << true << true << QString(
                                  "Product {\n"
                                  "    name: {\n"
                                  "        var foo = narf;\n"
                                  "        foo = foo + zort;\n"
                                  "        return foo;\n"
                                  "    }\n"
                                  "}\n");
    QTest::newRow("2 narfs, 1 zort") << true << true << QString(
                                  "Product {\n"
                                  "    name: {\n"
                                  "        var foo = narf;\n"
                                  "        foo = narf + foo + zort;\n"
                                  "        return foo;\n"
                                  "    }\n"
                                  "}\n");
}

void TestLanguage::identifierSearch()
{
    QFETCH(bool, expectedHasNarf);
    QFETCH(bool, expectedHasZort);
    QFETCH(QString, sourceCode);

    bool hasNarf = !expectedHasNarf;
    bool hasZort = !expectedHasZort;
    IdentifierSearch isearch;
    isearch.add("narf", &hasNarf);
    isearch.add("zort", &hasZort);

    QbsQmlJS::Engine engine;
    QbsQmlJS::Lexer lexer(&engine);
    lexer.setCode(sourceCode, 1);
    QbsQmlJS::Parser parser(&engine);
    QVERIFY(parser.parse());
    QVERIFY(parser.ast());
    isearch.start(parser.ast());
    QCOMPARE(hasNarf, expectedHasNarf);
    QCOMPARE(hasZort, expectedHasZort);
}

void TestLanguage::idUsage()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("idusage.qbs"));
        TopLevelProjectPtr project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 3);
        QVERIFY(products.contains("product1_1"));
        QVERIFY(products.contains("product2_2"));
        QVERIFY(products.contains("product3_3"));
    }
    catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QVERIFY(!exceptionCaught);
}

void TestLanguage::importCollection()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("import-collection/project.qbs"));
        const TopLevelProjectPtr project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        const ResolvedProductConstPtr product = products.value("da product");
        QCOMPARE(product->productProperties.value("targetName").toString(),
                 QLatin1String("C1f1C1f2C2f1C2f2"));
    }
    catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QVERIFY(!exceptionCaught);
}

void TestLanguage::invalidBindingInDisabledItem()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("invalidBindingInDisabledItem.qbs"));
        TopLevelProjectPtr project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 2);
    }
    catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QVERIFY(!exceptionCaught);
}

class JSSourceValueCreator
{
    FileContextPtr m_fileContext;
    QList<QString *> m_strings;
public:
    JSSourceValueCreator(const FileContextPtr &fileContext)
        : m_fileContext(fileContext)
    {
    }

    ~JSSourceValueCreator()
    {
        qDeleteAll(m_strings);
    }

    JSSourceValuePtr create(const QString &sourceCode)
    {
        JSSourceValuePtr value = JSSourceValue::create();
        value->setFile(m_fileContext);
        QString *str = new QString(sourceCode);
        m_strings += str;
        value->setSourceCode(QStringRef(str));
        return value;
    }
};

void TestLanguage::itemPrototype()
{
    FileContextPtr fileContext = FileContext::create();
    JSSourceValueCreator sourceValueCreator(fileContext);
    ItemPool pool;
    Item *proto = Item::create(&pool);
    proto->setProperty("x", sourceValueCreator.create("1"));
    proto->setProperty("y", sourceValueCreator.create("1"));
    Item *item = Item::create(&pool);
    item->setPrototype(proto);
    item->setProperty("y", sourceValueCreator.create("x + 1"));
    item->setProperty("z", sourceValueCreator.create("2"));

    Evaluator evaluator(m_engine, m_logger);
    QCOMPARE(evaluator.property(proto, "x").toVariant().toInt(), 1);
    QCOMPARE(evaluator.property(proto, "y").toVariant().toInt(), 1);
    QVERIFY(!evaluator.property(proto, "z").isValid());
    QCOMPARE(evaluator.property(item, "x").toVariant().toInt(), 1);
    QCOMPARE(evaluator.property(item, "y").toVariant().toInt(), 2);
    QCOMPARE(evaluator.property(item, "z").toVariant().toInt(), 2);
}

void TestLanguage::itemScope()
{
    FileContextPtr fileContext = FileContext::create();
    JSSourceValueCreator sourceValueCreator(fileContext);
    ItemPool pool;
    Item *scope1 = Item::create(&pool);
    scope1->setProperty("x", sourceValueCreator.create("1"));
    Item *scope2 = Item::create(&pool);
    scope2->setScope(scope1);
    scope2->setProperty("y", sourceValueCreator.create("x + 1"));
    Item *item = Item::create(&pool);
    item->setScope(scope2);
    item->setProperty("z", sourceValueCreator.create("x + y"));

    Evaluator evaluator(m_engine, m_logger);
    QCOMPARE(evaluator.property(scope1, "x").toVariant().toInt(), 1);
    QCOMPARE(evaluator.property(scope2, "y").toVariant().toInt(), 2);
    QVERIFY(!evaluator.property(scope2, "x").isValid());
    QCOMPARE(evaluator.property(item, "z").toVariant().toInt(), 3);
}

void TestLanguage::jsExtensions()
{
    QFile file(testProject("jsextensions.js"));
    QVERIFY(file.open(QFile::ReadOnly));
    QTextStream ts(&file);
    QString code = ts.readAll();
    QVERIFY(!code.isEmpty());
    QScriptValue evaluated = m_engine->evaluate(code, file.fileName(), 1);
    if (m_engine->hasErrorOrException(evaluated)) {
        qDebug() << m_engine->uncaughtExceptionBacktrace();
        QFAIL(qPrintable(m_engine->lastErrorString(evaluated)));
    }
}

void TestLanguage::jsImportUsedInMultipleScopes_data()
{
    QTest::addColumn<QString>("buildVariant");
    QTest::addColumn<QString>("expectedProductName");
    QTest::newRow("debug") << QString("debug") << QString("MyProduct_debug");
    QTest::newRow("release") << QString("release") << QString("MyProduct");
}

void TestLanguage::jsImportUsedInMultipleScopes()
{
    QFETCH(QString, buildVariant);
    QFETCH(QString, expectedProductName);

    bool exceptionCaught = false;
    try {
        SetupProjectParameters params = defaultParameters;
        params.setProjectFilePath(testProject("jsimportsinmultiplescopes.qbs"));
        params.setBuildVariant(buildVariant);
        params.expandBuildConfiguration();
        TopLevelProjectPtr project = loader->loadProject(params);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 1);
        ResolvedProductPtr product = products.values().first();
        QVERIFY(product);
        QCOMPARE(product->name, expectedProductName);
    }
    catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QVERIFY(!exceptionCaught);
}

void TestLanguage::moduleProperties_data()
{
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<QStringList>("expectedValues");
    QTest::newRow("init") << QString() << QStringList();
    QTest::newRow("merge_lists")
            << "defines"
            << (QStringList() << "THE_PRODUCT" << "QT_CORE" << "QT_GUI" << "QT_NETWORK");
    QTest::newRow("merge_lists_and_values")
            << "defines"
            << (QStringList() << "THE_PRODUCT" << "QT_CORE" << "QT_GUI" << "QT_NETWORK");
    QTest::newRow("merge_lists_with_duplicates")
            << "cxxFlags"
            << (QStringList() << "-foo" << "BAR" << "-foo" << "BAZ");
    QTest::newRow("merge_lists_with_prototype_values")
            << "rpaths"
            << (QStringList() << "/opt/qt/lib" << "$ORIGIN");
    QTest::newRow("list_property_that_references_product")
            << "listProp"
            << (QStringList() << "x" << "123");
    QTest::newRow("list_property_depending_on_overridden_property")
            << "listProp2"
            << (QStringList() << "PRODUCT_STUFF" << "DEFAULT_STUFF" << "EXTRA_STUFF");
    QTest::newRow("cleanup") << QString() << QStringList();
}

void TestLanguage::moduleProperties()
{
    HANDLE_INIT_CLEANUP_DATATAGS("moduleproperties.qbs");
    QFETCH(QString, propertyName);
    QFETCH(QStringList, expectedValues);
    QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
    const QString productName = QString::fromLocal8Bit(QTest::currentDataTag());
    ResolvedProductPtr product = products.value(productName);
    QVERIFY(product);
    QVariantList values = PropertyFinder().propertyValues(product->moduleProperties->value(),
                                                          "dummy", propertyName);
    QStringList valueStrings;
    foreach (const QVariant &v, values)
        valueStrings += v.toString();
    QEXPECT_FAIL("list_property_depending_on_overridden_property", "QBS-845", Continue);
    QCOMPARE(valueStrings, expectedValues);
}

void TestLanguage::moduleScope()
{
    class IntPropertyFinder
    {
        const QVariantMap &m_properties;
    public:
        IntPropertyFinder(const QVariantMap &properties)
            : m_properties(properties)
        {}

        int intValue(const QString &name)
        {
            return PropertyFinder().propertyValue(m_properties, "scopemod", name).toInt();
        }
    };

    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("modulescope.qbs"));
        TopLevelProjectPtr project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 1);
        ResolvedProductPtr product = products.value("product1");
        QVERIFY(product);
        IntPropertyFinder ipf(product->moduleProperties->value());
        QCOMPARE(ipf.intValue("a"), 2);     // overridden in module instance
        QCOMPARE(ipf.intValue("b"), 1);     // genuine
        QCOMPARE(ipf.intValue("c"), 3);     // genuine, dependent on overridden value
        QCOMPARE(ipf.intValue("d"), 2);     // genuine, dependent on genuine value
        QCOMPARE(ipf.intValue("e"), 1);     // genuine
        QCOMPARE(ipf.intValue("f"), 2);     // overridden
        QCOMPARE(ipf.intValue("g"), 156);   // overridden, dependent on product properties
        QCOMPARE(ipf.intValue("h"), 158);   // overridden, base dependent on product properties
    }
    catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);

}

void TestLanguage::modules_data()
{
    QTest::addColumn<QStringList>("expectedModulesInProduct");
    QTest::addColumn<QString>("expectedProductProperty");
    QTest::newRow("init") << QStringList();
    QTest::newRow("no_modules")
            << (QStringList() << "qbs")
            << QString();
    QTest::newRow("qt_core")
            << (QStringList() << "qbs" << "dummy" << "dummyqt.core")
            << QString("1.2.3");
    QTest::newRow("qt_gui")
            << (QStringList() << "qbs" << "dummy" << "dummyqt.core" << "dummyqt.gui")
            << QString("guiProperty");
    QTest::newRow("qt_gui_network")
            << (QStringList() << "qbs" << "dummy" << "dummyqt.core" << "dummyqt.gui"
                              << "dummyqt.network")
            << QString("guiProperty,networkProperty");
    QTest::newRow("deep_module_name")
            << (QStringList() << "qbs" << "dummy" << "dummy.deep.moat")
            << QString("abysmal");
    QTest::newRow("deep_module_name_submodule_syntax1")
            << (QStringList() << "qbs" << "dummy" << "dummy.deep.moat")
            << QString("abysmal");
    QTest::newRow("deep_module_name_submodule_syntax2")
            << (QStringList() << "qbs" << "dummy" << "dummy.deep.moat")
            << QString("abysmal");
    QTest::newRow("dummy_twice")
            << (QStringList() << "qbs" << "dummy")
            << QString();
    QTest::newRow("cleanup") << QStringList();
}

void TestLanguage::modules()
{
    HANDLE_INIT_CLEANUP_DATATAGS("modules.qbs");
    QFETCH(QStringList, expectedModulesInProduct);
    QFETCH(QString, expectedProductProperty);
    QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
    const QString productName = QString::fromLocal8Bit(QTest::currentDataTag());
    ResolvedProductPtr product = products.value(productName);
    QVERIFY(product);
    QCOMPARE(product->name, productName);
    QStringList modulesInProduct;
    foreach (ResolvedModuleConstPtr m, product->modules)
        modulesInProduct += m->name;
    modulesInProduct.sort();
    expectedModulesInProduct.sort();
    QCOMPARE(modulesInProduct, expectedModulesInProduct);
    QCOMPARE(product->productProperties.value("foo").toString(), expectedProductProperty);
}

void TestLanguage::outerInGroup()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("outerInGroup.qbs"));
        TopLevelProjectPtr project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 1);
        ResolvedProductPtr product = products.value("OuterInGroup");
        QVERIFY(product);
        QCOMPARE(product->groups.count(), 2);
        GroupPtr group = product->groups.at(0);
        QVERIFY(group);
        QCOMPARE(group->name, product->name);
        QCOMPARE(group->files.count(), 1);
        SourceArtifactConstPtr artifact = group->files.first();
        QVariant installDir = artifact->properties->qbsPropertyValue("installDir");
        QCOMPARE(installDir.toString(), QString("/somewhere"));
        group = product->groups.at(1);
        QVERIFY(group);
        QCOMPARE(group->name, QString("Special Group"));
        QCOMPARE(group->files.count(), 1);
        artifact = group->files.first();
        installDir = artifact->properties->qbsPropertyValue("installDir");
        QCOMPARE(installDir.toString(), QString("/somewhere/else"));
    }
    catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::pathProperties()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("pathproperties.qbs"));
        project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        ResolvedProductPtr product = products.value("product1");
        QVERIFY(product);
        QString projectFileDir = QFileInfo(defaultParameters.projectFilePath()).absolutePath();
        const QVariantMap productProps = product->productProperties;
        const QVariantMap moduleProps = product->moduleProperties->value();
        QCOMPARE(productProps.value("projectFileDir").toString(), projectFileDir);
        QStringList filesInProjectFileDir = QStringList()
                << FileInfo::resolvePath(projectFileDir, "aboutdialog.h")
                << FileInfo::resolvePath(projectFileDir, "aboutdialog.cpp");
        QCOMPARE(productProps.value("filesInProjectFileDir").toStringList(), filesInProjectFileDir);
        QStringList includePaths = getConfigProperty(moduleProps,
                QStringList() << "modules" << "dummy" << "includePaths").toStringList();
        QCOMPARE(includePaths, QStringList() << projectFileDir);
        QCOMPARE(productProps.value("base_fileInProductDir").toString(),
                 FileInfo::resolvePath(projectFileDir, QLatin1String("foo")));
        QCOMPARE(productProps.value("base_fileInBaseProductDir").toString(),
                 FileInfo::resolvePath(projectFileDir, QLatin1String("subdir/bar")));
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::profileValuesAndOverriddenValues()
{
    bool exceptionCaught = false;
    try {
        Settings settings((QString()));
        TemporaryProfile tp(QLatin1String("tst_lang_profile"), &settings);
        Profile profile = tp.p;
        profile.setValue("dummy.defines", "IN_PROFILE");
        profile.setValue("dummy.cFlags", "IN_PROFILE");
        profile.setValue("dummy.cxxFlags", "IN_PROFILE");
        profile.setValue("qbs.architecture", "x86");
        SetupProjectParameters parameters = defaultParameters;
        parameters.setTopLevelProfile(profile.name());
        QVariantMap overriddenValues;
        overriddenValues.insert("dummy.cFlags", "OVERRIDDEN");
        parameters.setOverriddenValues(overriddenValues);
        parameters.setProjectFilePath(testProject("profilevaluesandoverriddenvalues.qbs"));
        parameters.expandBuildConfiguration();
        project = loader->loadProject(parameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        ResolvedProductPtr product = products.value("product1");
        QVERIFY(product);
        PropertyFinder pf;
        QVariantList values;
        values = pf.propertyValues(product->moduleProperties->value(), "dummy", "cxxFlags");
        QCOMPARE(values.length(), 1);
        QCOMPARE(values.first().toString(), QString("IN_PROFILE"));
        values = pf.propertyValues(product->moduleProperties->value(), "dummy", "defines");
        QCOMPARE(values, QVariantList() << QLatin1String("IN_FILE") << QLatin1String("IN_PROFILE"));
        values = pf.propertyValues(product->moduleProperties->value(), "dummy", "cFlags");
        QCOMPARE(values.length(), 1);
        QCOMPARE(values.first().toString(), QString("OVERRIDDEN"));
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::productConditions()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("productconditions.qbs"));
        TopLevelProjectPtr project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 4);
        ResolvedProductPtr product;
        product = products.value("product_no_condition");
        QVERIFY(product);
        QVERIFY(product->enabled);

        product = products.value("product_true_condition");
        QVERIFY(product);
        QVERIFY(product->enabled);

        product = products.value("product_condition_dependent_of_module");
        QVERIFY(product);
        QVERIFY(product->enabled);

        product = products.value("product_false_condition");
        QVERIFY(product);
        QVERIFY(!product->enabled);
    }
    catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::productDirectories()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("productdirectories.qbs"));
        ResolvedProjectPtr project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 1);
        ResolvedProductPtr product;
        product = products.value("MyApp");
        QVERIFY(product);
        const QVariantMap config = product->productProperties;
        QCOMPARE(config.value(QLatin1String("buildDirectory")).toString(),
                 product->buildDirectory());
        QCOMPARE(config.value(QLatin1String("sourceDirectory")).toString(), testDataDir());
    }
    catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::propertiesBlocks_data()
{
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<QStringList>("expectedValues");
    QTest::addColumn<QString>("expectedStringValue");

    QTest::newRow("init") << QString() << QStringList() << QString();
    QTest::newRow("property_overwrite")
            << QString("dummy.defines")
            << QStringList("OVERWRITTEN")
            << QString();
    QTest::newRow("property_overwrite_no_outer")
            << QString("dummy.defines")
            << QStringList("OVERWRITTEN")
            << QString();
    QTest::newRow("property_append_to_outer")
            << QString("dummy.defines")
            << (QStringList() << QString("ONE") << QString("TWO"))
            << QString();

    QTest::newRow("multiple_exclusive_properties")
            << QString("dummy.defines")
            << QStringList("OVERWRITTEN")
            << QString();
    QTest::newRow("multiple_exclusive_properties_no_outer")
            << QString("dummy.defines")
            << QStringList("OVERWRITTEN")
            << QString();
    QTest::newRow("multiple_exclusive_properties_append_to_outer")
            << QString("dummy.defines")
            << (QStringList() << QString("ONE") << QString("TWO"))
            << QString();

    QTest::newRow("condition_refers_to_product_property")
            << QString("dummy.defines")
            << QStringList("OVERWRITTEN")
            << QString("OVERWRITTEN");
    QTest::newRow("condition_refers_to_project_property")
            << QString("dummy.defines")
            << QStringList("OVERWRITTEN")
            << QString("OVERWRITTEN");

    QTest::newRow("ambiguous_properties")
            << QString("dummy.defines")
            << (QStringList() << QString("ONE") << QString("TWO"))
            << QString();
    QTest::newRow("inheritance_overwrite_in_subitem")
            << QString("dummy.defines")
            << (QStringList() << QString("OVERWRITTEN_IN_SUBITEM"))
            << QString();
    QTest::newRow("inheritance_retain_base1")
            << QString("dummy.defines")
            << (QStringList() << QString("BASE") << QString("SUB"))
            << QString();
    QTest::newRow("inheritance_retain_base2")
            << QString("dummy.defines")
            << (QStringList() << QString("BASE") << QString("SUB"))
            << QString();
    QTest::newRow("inheritance_retain_base3")
            << QString("dummy.defines")
            << (QStringList() << QString("BASE") << QString("SUB"))
            << QString();
    QTest::newRow("inheritance_retain_base4")
            << QString("dummy.defines")
            << (QStringList() << QString("BASE"))
            << QString();
    QTest::newRow("inheritance_condition_in_subitem1")
            << QString("dummy.defines")
            << (QStringList() << QString("SOMETHING") << QString("SUB"))
            << QString();
    QTest::newRow("inheritance_condition_in_subitem2")
            << QString("dummy.defines")
            << (QStringList() << QString("SOMETHING"))
            << QString();
    QTest::newRow("condition_references_id")
            << QString("dummy.defines")
            << (QStringList() << QString("OVERWRITTEN"))
            << QString();
    QTest::newRow("conditional-depends")
            << QString("dummy.defines")
            << QStringList()
            << QString();
    QTest::newRow("cleanup") << QString() << QStringList() << QString();
}

void TestLanguage::propertiesBlocks()
{
    HANDLE_INIT_CLEANUP_DATATAGS("propertiesblocks.qbs");
    QFETCH(QString, propertyName);
    QFETCH(QStringList, expectedValues);
    QFETCH(QString, expectedStringValue);
    QVERIFY(project);
    QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
    const QString productName = QString::fromLocal8Bit(QTest::currentDataTag());
    ResolvedProductPtr product = products.value(productName);
    QVERIFY(product);
    QCOMPARE(product->name, productName);
    QVariant v = productPropertyValue(product, propertyName);
    QCOMPARE(v.toStringList(), expectedValues);
    if (!expectedStringValue.isEmpty()) {
        v = productPropertyValue(product, "someString");
        QCOMPARE(v.toString(), expectedStringValue);
    }
}

void TestLanguage::propertiesBlockInGroup()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("properties-block-in-group.qbs"));
        const TopLevelProjectPtr project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        QCOMPARE(project->allProducts().count(), 1);
        const ResolvedProductConstPtr product = project->allProducts().first();
        const auto groupIt = std::find_if(product->groups.constBegin(), product->groups.constEnd(),
                [](const GroupConstPtr &g) { return g->name == "the group"; });
        QVERIFY(groupIt != product->groups.constEnd());
        const QVariantMap propertyMap = (*groupIt)->properties->value();
        const QVariantList value = PropertyFinder().propertyValues(propertyMap, "dummy", "defines");
        QStringList stringListValue;
        std::transform(value.constBegin(), value.constEnd(), std::back_inserter(stringListValue),
                       [](const QVariant &v) { return v.toString(); });
        QCOMPARE(stringListValue, QStringList() << "BASEDEF" << "FEATURE_ENABLED");
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::qbsPropertiesInProjectCondition()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(
                    testProject("qbs-properties-in-project-condition.qbs"));
        const TopLevelProjectPtr project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 0);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::qualifiedId()
{
    QString str = "foo.bar.baz";
    QualifiedId id = QualifiedId::fromString(str);
    QCOMPARE(id.count(), 3);
    QCOMPARE(id.toString(), str);

    id = QualifiedId("blubb.di.blubb"); // c'tor does not split
    QCOMPARE(id.count(), 1);

    QList<QualifiedId> ids;
    ids << QualifiedId::fromString("a")
        << QualifiedId::fromString("a.a")
        << QualifiedId::fromString("b")
        << QualifiedId::fromString("c.a")
        << QualifiedId::fromString("c.b.a")
        << QualifiedId::fromString("c.c");
    QList<QualifiedId> sorted = ids;
    std::sort(sorted.begin(), sorted.end());
    QCOMPARE(ids, sorted);
}

void TestLanguage::recursiveProductDependencies()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(
                    testProject("recursive-dependencies/recursive-dependencies.qbs"));
        const TopLevelProjectPtr project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 4);
        const ResolvedProductConstPtr p1 = products.value("p1");
        QVERIFY(p1);
        const ResolvedProductConstPtr p2 = products.value("p2");
        QVERIFY(p2);
        const ResolvedProductPtr p3 = products.value("p3");
        QVERIFY(p3);
        const ResolvedProductPtr p4 = products.value("p4");
        QVERIFY(p4);
        QVERIFY(p1->dependencies == QSet<ResolvedProductPtr>() << p3 << p4);
        QVERIFY(p2->dependencies == QSet<ResolvedProductPtr>() << p3 << p4);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::fileTags_data()
{
    QTest::addColumn<int>("numberOfGroups");
    QTest::addColumn<QStringList>("expectedFileTags");

    QTest::newRow("init") << 0 << QStringList();
    QTest::newRow("filetagger_project_scope") << 1 << (QStringList() << "cpp");
    QTest::newRow("filetagger_product_scope") << 1 << (QStringList() << "asm");
    QTest::newRow("filetagger_static_pattern") << 1 << (QStringList() << "yellow");
    QTest::newRow("unknown_file_tag") << 1 << (QStringList() << "unknown-file-tag");
    QTest::newRow("set_file_tag_via_group") << 2 << (QStringList() << "c++");
    QTest::newRow("override_file_tag_via_group") << 2 << (QStringList() << "c++");
    QTest::newRow("add_file_tag_via_group") << 2 << (QStringList() << "cpp" << "zzz");
    QTest::newRow("cleanup") << 0 << QStringList();
}

void TestLanguage::fileTags()
{
    HANDLE_INIT_CLEANUP_DATATAGS("filetags.qbs");
    QFETCH(int, numberOfGroups);
    QFETCH(QStringList, expectedFileTags);
    QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
    ResolvedProductPtr product;
    const QString productName = QString::fromLocal8Bit(QTest::currentDataTag());
    QVERIFY(product = products.value(productName));
    QCOMPARE(product->groups.count(), numberOfGroups);
    GroupPtr group = product->groups.last();
    QVERIFY(group);
    QCOMPARE(group->files.count(), 1);
    SourceArtifactConstPtr sourceFile = group->files.first();
    QStringList fileTags = sourceFile->fileTags.toStringList();
    fileTags.sort();
    QCOMPARE(fileTags, expectedFileTags);
}

void TestLanguage::wildcards_data()
{
    QTest::addColumn<bool>("useGroup");
    QTest::addColumn<QStringList>("filesToCreate");
    QTest::addColumn<QString>("projectFileSubDir");
    QTest::addColumn<QString>("prefix");
    QTest::addColumn<QStringList>("patterns");
    QTest::addColumn<QStringList>("excludePatterns");
    QTest::addColumn<QStringList>("expected");

    const bool useGroup = true;
    for (int i = 0; i <= 1; ++i) {
        const bool useGroup = i;
        const QByteArray dataTagSuffix = useGroup ? " group" : " nogroup";
        QTest::newRow(QByteArray("simple 1") + dataTagSuffix)
                << useGroup
                << (QStringList() << "foo.h" << "foo.cpp" << "bar.h" << "bar.cpp")
                << QString()
                << QString()
                << (QStringList() << "*.h")
                << QStringList()
                << (QStringList() << "foo.h" << "bar.h");
        QTest::newRow(QByteArray("simple 2") + dataTagSuffix)
                << useGroup
                << (QStringList() << "foo.h" << "foo.cpp" << "bar.h" << "bar.cpp")
                << QString()
                << QString()
                << (QStringList() << "foo.*")
                << QStringList()
                << (QStringList() << "foo.h" << "foo.cpp");
        QTest::newRow(QByteArray("simple 3") + dataTagSuffix)
                << useGroup
                << (QStringList() << "foo.h" << "foo.cpp" << "bar.h" << "bar.cpp")
                << QString()
                << QString()
                << (QStringList() << "*.h" << "*.cpp")
                << QStringList()
                << (QStringList() << "foo.h" << "foo.cpp" << "bar.h" << "bar.cpp");
        QTest::newRow(QByteArray("exclude 1") + dataTagSuffix)
                << useGroup
                << (QStringList() << "foo.h" << "foo.cpp" << "bar.h" << "bar.cpp")
                << QString()
                << QString()
                << (QStringList() << "*.h" << "*.cpp")
                << (QStringList() << "bar*")
                << (QStringList() << "foo.h" << "foo.cpp");
        QTest::newRow(QByteArray("exclude 2") + dataTagSuffix)
                << useGroup
                << (QStringList() << "foo.h" << "foo.cpp" << "bar.h" << "bar.cpp")
                << QString()
                << QString()
                << (QStringList() << "*")
                << (QStringList() << "*.qbs")
                << (QStringList() << "foo.h" << "foo.cpp" << "bar.h" << "bar.cpp");
        QTest::newRow(QByteArray("non-recursive") + dataTagSuffix)
                << useGroup
                << (QStringList() << "a/foo.h" << "a/foo.cpp" << "a/b/bar.h" << "a/b/bar.cpp")
                << QString()
                << QString()
                << (QStringList() << "a/*")
                << QStringList()
                << (QStringList() << "a/foo.h" << "a/foo.cpp");
        QTest::newRow(QByteArray("absolute paths") + dataTagSuffix)
                << useGroup
                << (QStringList() << "foo.h" << "foo.cpp" << "bar.h" << "bar.cpp")
                << QString()
                << QString()
                << (QStringList() << m_wildcardsTestDirPath + "/?oo.*")
                << QStringList()
                << (QStringList() << "foo.h" << "foo.cpp");
        QTest::newRow(QByteArray("relative paths with dotdot") + dataTagSuffix)
                << useGroup
                << (QStringList() << "bar.h" << "bar.cpp")
                << QString("TheLaughingLlama")
                << QString()
                << (QStringList() << "../bar.*")
                << QStringList()
                << (QStringList() << "bar.h" << "bar.cpp");
    }
    QTest::newRow(QByteArray("recursive 1"))
            << useGroup
            << (QStringList() << "a/foo.h" << "a/foo.cpp" << "a/b/bar.h" << "a/b/bar.cpp")
            << QString()
            << QString()
            << (QStringList() << "a/**")
            << QStringList()
            << (QStringList() << "a/foo.h" << "a/foo.cpp" << "a/b/bar.h" << "a/b/bar.cpp");
    QTest::newRow(QByteArray("recursive 2"))
            << useGroup
            << (QStringList()
                << "d/1.h" << "b/d/1.h" << "b/c/d/1.h"
                << "d/e/1.h" << "b/d/e/1.h" << "b/c/d/e/1.h"
                << "a/d/1.h" << "a/b/d/1.h" << "a/b/c/d/1.h"
                << "a/d/e/1.h" << "a/b/d/e/1.h" << "a/b/c/d/e/1.h"
                << "a/d/1.cpp" << "a/b/d/1.cpp" << "a/b/c/d/1.h"
                << "a/d/e/1.cpp" << "a/b/d/e/1.cpp" << "a/b/c/d/e/1.cpp")
            << QString()
            << QString()
            << (QStringList() << "a/**/d/*.h")
            << QStringList()
            << (QStringList() << "a/d/1.h" << "a/b/d/1.h" << "a/b/c/d/1.h");
    QTest::newRow(QByteArray("recursive 3"))
            << useGroup
            << (QStringList() << "a/foo.h" << "a/foo.cpp" << "a/b/bar.h" << "a/b/bar.cpp")
            << QString()
            << QString()
            << (QStringList() << "a/**/**/**")
            << QStringList()
            << (QStringList() << "a/foo.h" << "a/foo.cpp" << "a/b/bar.h" << "a/b/bar.cpp");
    QTest::newRow(QByteArray("prefix"))
            << useGroup
            << (QStringList() << "subdir/foo.h" << "subdir/foo.cpp" << "subdir/bar.h"
                << "subdir/bar.cpp")
            << QString()
            << QString("subdir/")
            << (QStringList() << "*.h")
            << QStringList()
            << (QStringList() << "subdir/foo.h" << "subdir/bar.h");
    QTest::newRow(QByteArray("non-existing absolute path"))
            << useGroup
            << QStringList()
            << QString()
            << QString("/dir")
            << (QStringList() << "*.whatever")
            << QStringList()
            << QStringList();
}

void TestLanguage::wildcards()
{
    QFETCH(bool, useGroup);
    QFETCH(QStringList, filesToCreate);
    QFETCH(QString, projectFileSubDir);
    QFETCH(QString, prefix);
    QFETCH(QStringList, patterns);
    QFETCH(QStringList, excludePatterns);
    QFETCH(QStringList, expected);

    // create test directory
    QDir::setCurrent(QDir::tempPath());
    {
        QString errorMessage;
        if (QFile::exists(m_wildcardsTestDirPath)) {
            if (!removeDirectoryWithContents(m_wildcardsTestDirPath, &errorMessage)) {
                qDebug() << errorMessage;
                QVERIFY2(false, "removeDirectoryWithContents failed");
            }
        }
        QVERIFY(QDir().mkdir(m_wildcardsTestDirPath));
    }

    // create project file
    const QString groupName = "Keks";
    QString dataTag = QString::fromLocal8Bit(QTest::currentDataTag());
    dataTag.replace(' ', '_');
    if (!projectFileSubDir.isEmpty()) {
        if (!projectFileSubDir.startsWith('/'))
            projectFileSubDir.prepend('/');
        if (projectFileSubDir.endsWith('/'))
            projectFileSubDir.chop(1);
        QVERIFY(QDir().mkpath(m_wildcardsTestDirPath + projectFileSubDir));
    }
    const QString projectFilePath = m_wildcardsTestDirPath + projectFileSubDir + "/test_" + dataTag
            + ".qbs";
    {
        QFile projectFile(projectFilePath);
        QVERIFY(projectFile.open(QIODevice::WriteOnly));
        QTextStream s(&projectFile);
        s << "import qbs.base 1.0" << endl << endl
          << "Application {" << endl
          << "  name: \"MyProduct\"" << endl;
        if (useGroup) {
            s << "  Group {" << endl
              << "     name: " << toJSLiteral(groupName) << endl;
        }
        if (!prefix.isEmpty())
            s << "  prefix: " << toJSLiteral(prefix) << endl;
        if (!patterns.isEmpty())
            s << "  files: " << toJSLiteral(patterns) << endl;
        if (!excludePatterns.isEmpty())
            s << "  excludeFiles: " << toJSLiteral(excludePatterns) << endl;
        if (useGroup)
            s << "  }" << endl;
        s << "}" << endl << endl;
    }

    // create files
    foreach (QString filePath, filesToCreate) {
        filePath.prepend(m_wildcardsTestDirPath + '/');
        QFileInfo fi(filePath);
        if (!QDir(fi.path()).exists())
            QVERIFY(QDir().mkpath(fi.path()));
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    // read the project
    bool exceptionCaught = false;
    ResolvedProductPtr product;
    try {
        defaultParameters.setProjectFilePath(projectFilePath);
        project = loader->loadProject(defaultParameters);
        QVERIFY(project);
        const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        product = products.value("MyProduct");
        QVERIFY(product);
        GroupPtr group;
        if (useGroup) {
            QCOMPARE(product->groups.count(), 2);
            foreach (const GroupPtr &rg, product->groups) {
                if (rg->name == groupName) {
                    group = rg;
                    break;
                }
            }
        } else {
            QCOMPARE(product->groups.count(), 1);
            group = product->groups.first();
        }
        QVERIFY(group);
        QCOMPARE(group->files.count(), 0);
        SourceWildCards::Ptr wildcards = group->wildcards;
        QVERIFY(wildcards);
        QStringList actualFilePaths;
        foreach (const SourceArtifactConstPtr &artifact, wildcards->files) {
            QString str = artifact->absoluteFilePath;
            int idx = str.indexOf(m_wildcardsTestDirPath);
            if (idx != -1)
                str.remove(0, idx + m_wildcardsTestDirPath.count() + 1);
            actualFilePaths << str;
        }
        actualFilePaths.sort();
        expected.sort();
        QCOMPARE(actualFilePaths, expected);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

} // namespace Internal
} // namespace qbs
