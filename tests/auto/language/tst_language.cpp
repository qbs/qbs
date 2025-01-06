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

#undef QT_NO_CAST_FROM_ASCII // I am qmake, and I approve this hack.

#include "tst_language.h"

#include "../shared.h"

#include <app/shared/logging/consolelogger.h>
#include <language/evaluator.h>
#include <language/filecontext.h>
#include <language/identifiersearch.h>
#include <language/item.h>
#include <language/itempool.h>
#include <language/language.h>
#include <language/propertymapinternal.h>
#include <language/scriptengine.h>
#include <language/value.h>
#include <loader/projectresolver.h>
#include <parser/qmljslexer_p.h>
#include <parser/qmljsparser_p.h>
#include <tools/scripttools.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <tools/jsliterals.h>
#include <tools/profile.h>
#include <tools/qttools.h>
#include <tools/settings.h>
#include <tools/stlutils.h>

#include <QtCore/qjsonobject.h>
#include <QtCore/qprocess.h>

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

Q_DECLARE_METATYPE(QList<bool>)

using namespace qbs;
using namespace qbs::Internal;

static QString testDataDir() {
    return testDataSourceDir(SRCDIR "/testdata");
}
static QString testProject(const char *fileName) {
    return testDataDir() + QLatin1Char('/') + QLatin1String(fileName);
}

class JSSourceValueCreator
{
    FileContextPtr m_fileContext;
    std::vector<std::unique_ptr<QString>> m_strings;
public:
    JSSourceValueCreator(const FileContextPtr &fileContext)
        : m_fileContext(fileContext)
    {
    }

    JSSourceValuePtr create(const QString &sourceCode)
    {
        JSSourceValuePtr value = JSSourceValue::create();
        value->setFile(m_fileContext);
        auto str = std::make_unique<QString>(sourceCode);
        value->setSourceCode(*str.get());
        m_strings.push_back(std::move(str));
        return value;
    }
};

TestLanguage::TestLanguage(ILogSink *logSink, Settings *settings)
    : m_logSink(logSink)
    , m_settings(settings)
    , m_wildcardsTestDirPath(QDir::tempPath() + QLatin1String("/_wildcards_test_dir_"))
{
    m_rand.seed(QTime::currentTime().msec());
    qRegisterMetaType<QList<bool> >("QList<bool>");
}

TestLanguage::~TestLanguage() = default;

QHash<QString, ResolvedProductPtr> TestLanguage::productsFromProject(ResolvedProjectPtr project)
{
    QHash<QString, ResolvedProductPtr> result;
    const auto products = project->allProducts();
    for (const ResolvedProductPtr &product : products)
        result.insert(product->name, product);
    return result;
}

template <typename C>
typename C::value_type findByName(const C &container, const QString &name)
{
    auto endIt = std::end(container);
    auto it = std::find_if(std::begin(container), endIt,
                           [&name] (const typename C::value_type &thing) {
        return thing->name == name;
    });
    if (it != endIt)
        return *it;
    return typename C::value_type();
}

ResolvedModuleConstPtr TestLanguage::findModuleByName(ResolvedProductPtr product, const QString &name)
{
    return findByName(product->modules, name);
}

QVariant TestLanguage::productPropertyValue(ResolvedProductPtr product, QString propertyName)
{
    QStringList propertyNameComponents = propertyName.split(QLatin1Char('.'));
    if (propertyNameComponents.size() > 1)
        return product->moduleProperties->property(propertyNameComponents);
    return getConfigProperty(product->productProperties, propertyNameComponents);
}

void TestLanguage::handleInitCleanupDataTags(const char *projectFileName, bool *handled)
{
    const QByteArray dataTag = QTest::currentDataTag();
    if (dataTag == "init") {
        *handled = true;
        bool exceptionCaught = false;
        try {
            resolveProject(projectFileName);
            QVERIFY(!!project);
        } catch (const ErrorInfo &e) {
            exceptionCaught = true;
            qDebug() << e.toString();
        }
        QCOMPARE(exceptionCaught, false);
    } else if (dataTag == "cleanup") {
        *handled = true;
        project.reset();
    } else {
        *handled = false;
    }
}

TopLevelProjectPtr TestLanguage::resolveProject(const char *relProjectFilePath)
{
    if (relProjectFilePath)
        defaultParameters.setProjectFilePath(testProject(relProjectFilePath));
    defaultParameters.expandBuildConfiguration();
    ProjectResolver resolver(defaultParameters, m_engine.get(), m_logger);
    return project = resolver.resolve();
}

void TestLanguage::init()
{
    // clear caches, otherwise StoredVariantValues may end up being at the same address
    // as the destroyed value
    m_engine->reset();
    m_logSink->setLogLevel(LoggerInfo);
    defaultParameters = {};
    defaultParameters.setBuildRoot(m_tempDir.path() + "/buildroot");
    defaultParameters.setPropertyCheckingMode(ErrorHandlingMode::Strict);
    defaultParameters.setSettingsDirectory(m_settings->baseDirectory());
    defaultParameters.setTopLevelProfile(profileName());
    defaultParameters.setMaxJobCount(1);
    defaultParameters.setConfigurationName("default");
    defaultParameters.setEnvironment(QProcessEnvironment::systemEnvironment());
    defaultParameters.setSearchPaths({SRCDIR "/../../../share/qbs"});

    QVERIFY(m_tempDir.isValid());
}

#define HANDLE_INIT_CLEANUP_DATATAGS(fn) {\
    bool handled;\
    handleInitCleanupDataTags(fn, &handled);\
    if (handled)\
        return;\
    QVERIFY(!!project);\
}

void TestLanguage::initTestCase()
{
    m_logger = Logger(m_logSink);
    m_engine = ScriptEngine::create(m_logger, EvalContext::PropertyEvaluation);

    QVERIFY(QFileInfo(m_wildcardsTestDirPath).isAbsolute());
}

void TestLanguage::additionalProductTypes()
{
    bool exceptionCaught = false;
    try {
        resolveProject("additional-product-types.qbs");
        QVERIFY(!!project);
        const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        const ResolvedProductConstPtr product = products.value("p");
        QVERIFY(!!product);
        const QVariantMap cfg = product->productProperties;
        QVERIFY(cfg.value("hasTag1").toBool());
        QVERIFY(cfg.value("hasTag2").toBool());
        QVERIFY(cfg.value("hasTag3").toBool());
        QVERIFY(!cfg.value("hasTag4").toBool());
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::baseProperty()
{
    bool exceptionCaught = false;
    try {
        resolveProject("baseproperty.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        ResolvedProductPtr product = products.value("product1");
        QVERIFY(!!product);
        QVariantMap cfg = product->productProperties;
        QCOMPARE(cfg.value("narf").toStringList(), QStringList() << "boo");
        QCOMPARE(cfg.value("zort").toStringList(), QStringList() << "bar" << "boo");
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::baseValidation()
{
    try {
        resolveProject("base-validate/base-validate.qbs");
        QVERIFY2(false, "exception expected");
    } catch (const qbs::ErrorInfo &e) {
        QVERIFY2(e.toString().contains("Parent succeeded, child failed."),
                 qPrintable(e.toString()));
    }
}

void TestLanguage::brokenDependencyCycle()
{
    QFETCH(QString, projectFileName);
    try {
        resolveProject(qPrintable(projectFileName));
    } catch (const qbs::ErrorInfo &e) {
        QVERIFY2(false, qPrintable(e.toString()));
    }
}

void TestLanguage::brokenDependencyCycle_data()
{
    QTest::addColumn<QString>("projectFileName");
    QTest::newRow("one order of products") << "broken-dependency-cycle1.qbs";
    QTest::newRow("another order of products") << "broken-dependency-cycle2.qbs";
}

void TestLanguage::buildConfigStringListSyntax()
{
    bool exceptionCaught = false;
    try {
        QVariantMap overriddenValues;
        overriddenValues.insert("project.someStrings", "foo,bar,baz");
        defaultParameters.setOverriddenValues(overriddenValues);
        resolveProject("buildconfigstringlistsyntax.qbs");
        QVERIFY(!!project);
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
        QVERIFY(resolveProject("builtinFunctionInSearchPathsProperty.qbs"));
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::chainedProbes()
{
    bool exceptionCaught = false;
    try {
        resolveProject("chained-probes/chained-probes.qbs");
        QVERIFY(!!project);
        QCOMPARE(project->products.size(), size_t(1));
        const QString prop1Val = project->products.front()->moduleProperties
                ->moduleProperty("m", "prop1").toString();
        QCOMPARE(prop1Val, QLatin1String("probe1Val"));
        const QString prop2Val = project->products.front()->moduleProperties
                ->moduleProperty("m", "prop2").toString();
        QCOMPARE(prop2Val, QLatin1String("probe1Valprobe2Val"));
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);

}

void TestLanguage::versionCompare()
{
    bool exceptionCaught = false;
    try {
        QVERIFY(resolveProject("versionCompare.qbs"));
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
        resolveProject("canonicalArchitecture.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        ResolvedProductPtr product = products.value(QStringLiteral("x86"));
        QVERIFY(!!product);
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
        resolveProject("rfc1034identifier.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        ResolvedProductPtr product = products.value(QStringLiteral("this-has-special-characters-"
                                                                  "uh-oh-Undersc0r3s-Are.Bad"));
        QVERIFY(!!product);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::throwThings_data()
{
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("result");
    QTest::addRow("bool") << "bool" << "true";
    QTest::addRow("int") << "int" << "43";
    QTest::addRow("string") << "string" << "an error";
    QTest::addRow("list") << "list" << R"([
    "an",
    "error"
])";
    QTest::addRow("object") << "object" << R"({
    "reason": "overheating",
    "result": "crash"
})";
}

void TestLanguage::throwThings()
{
    QFETCH(QString, type);
    QFETCH(QString, result);
    bool exceptionCaught = false;
    try {
        defaultParameters.setOverriddenValues({{"project.throwType", type}});
        resolveProject("throw.qbs");
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        QVERIFY2(e.toString().contains(result), qPrintable(e.toString()));
    }
    QVERIFY(exceptionCaught);
}

void TestLanguage::conditionalDepends()
{
    bool exceptionCaught = false;
    ResolvedProductPtr product;
    ResolvedModuleConstPtr dependency;
    try {
        defaultParameters.setOverriddenValues({std::make_pair(QString("products."
                                    "multilevel_module_props_overridden.dummy3.loadDummy"), true)});
        resolveProject("conditionaldepends.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);

        product = products.value("conditionaldepends_derived");
        QVERIFY(!!product);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(!!dependency);

        product = products.value("conditionaldepends_derived_false");
        QVERIFY(!!product);
        dependency = findModuleByName(product, "dummy");
        QCOMPARE(dependency, ResolvedModuleConstPtr());

        product = products.value("product_props_true");
        QVERIFY(!!product);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(!!dependency);

        product = products.value("product_props_false");
        QVERIFY(!!product);
        dependency = findModuleByName(product, "dummy");
        QCOMPARE(dependency, ResolvedModuleConstPtr());

        product = products.value("project_props_true");
        QVERIFY(!!product);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(!!dependency);

        product = products.value("project_props_false");
        QVERIFY(!!product);
        dependency = findModuleByName(product, "dummy");
        QCOMPARE(dependency, ResolvedModuleConstPtr());

        product = products.value("module_props_true");
        QVERIFY(!!product);
        dependency = findModuleByName(product, "dummy2");
        QVERIFY(!!dependency);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(!!dependency);

        product = products.value("module_props_false");
        QVERIFY(!!product);
        dependency = findModuleByName(product, "dummy2");
        QVERIFY(!!dependency);
        dependency = findModuleByName(product, "dummy");
        QCOMPARE(dependency, ResolvedModuleConstPtr());

        product = products.value("multilevel_module_props_true");
        QVERIFY(!!product);
        dependency = findModuleByName(product, "dummy3");
        QVERIFY(!!dependency);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(!!dependency);

        product = products.value("multilevel_module_props_false");
        QVERIFY(!!product);
        dependency = findModuleByName(product, "dummy3");
        QVERIFY(!!dependency);
        dependency = findModuleByName(product, "dummy");
        QCOMPARE(dependency, ResolvedModuleConstPtr());

        product = products.value("multilevel_module_props_overridden");
        QVERIFY(!!product);
        dependency = findModuleByName(product, "dummy3");
        QVERIFY(!!dependency);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(!!dependency);

        product = products.value("multilevel2_module_props_true");
        QVERIFY(!!product);
        dependency = findModuleByName(product, "dummy3_loader");
        QVERIFY(!!dependency);
        dependency = findModuleByName(product, "dummy3");
        QVERIFY(!!dependency);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(!!dependency);

        product = products.value("contradictory_conditions1");
        QVERIFY(!!product);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(!!dependency);

        product = products.value("contradictory_conditions2");
        QVERIFY(!!product);
        dependency = findModuleByName(product, "dummy");
        QVERIFY(!!dependency);

        product = products.value("unknown_dependency_condition_false");
        QVERIFY(!!product);
        dependency = findModuleByName(product, "doesonlyexistifhellfreezesover");
        QVERIFY(!dependency);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::convertStringList()
{
    FileContextPtr fileContext = FileContext::create();
    fileContext->setFilePath("/dev/null");
    JSSourceValueCreator sourceValueCreator(fileContext);
    ItemPool pool;
    Item *scope = Item::create(&pool, ItemType::Scope);
    scope->setProperty("x", sourceValueCreator.create("[\"a\", \"b\"]"));

    Evaluator evaluator(m_engine.get());
    auto variantValue = evaluator.variantValue(scope, "x");
    // despite we have a stringList prop, we evaluate it as a QVariantList
    QCOMPARE(variantValue.userType(), QMetaType::Type::QVariantList);
    // and we have to convert it explicitly
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    variantValue.convert(QMetaType(QMetaType::QStringList));
#else
    variantValue.convert(QMetaType::QStringList);
#endif
    QCOMPARE(variantValue.userType(), QMetaType::Type::QStringList);
    QCOMPARE(variantValue, QStringList({"a", "b"}));
}

void TestLanguage::correctDeclInParentLookup()
{
    ErrorInfo exception;
    try {
        resolveProject("correct-decl-in-parent-lookup.qbs");
        QVERIFY(project);
        QCOMPARE(project->products.size(), size_t(1));
        const ResolvedProductConstPtr theProduct = productsFromProject(project).value("p");
        QVERIFY(theProduct);
        QCOMPARE(
            theProduct->moduleProperties->moduleProperty("dummy", "cxxFlags").toStringList(),
            QStringList("x"));
    } catch (const ErrorInfo &e) {
        exception = e;
    }
    QVERIFY2(!exception.hasError(), qPrintable(exception.toString()));
}

void TestLanguage::delayedError()
{
    QFETCH(bool, productEnabled);
    try {
        QFETCH(QString, projectFileName);
        QVariantMap overriddenValues;
        overriddenValues.insert("project.enableProduct", productEnabled);
        defaultParameters.setOverriddenValues(overriddenValues);
        resolveProject(projectFileName.toLatin1());
        QCOMPARE(productEnabled, false);
        QVERIFY(!!project);
        QCOMPARE(project->products.size(), size_t(1));
        const ResolvedProductConstPtr theProduct = productsFromProject(project).value("theProduct");
        QVERIFY(!!theProduct);
        QCOMPARE(theProduct->enabled, false);
    } catch (const ErrorInfo &e) {
        if (!productEnabled)
            qDebug() << e.toString();
        QCOMPARE(productEnabled, true);
    }
}

void TestLanguage::delayedError_data()
{
    QTest::addColumn<QString>("projectFileName");
    QTest::addColumn<bool>("productEnabled");
    QTest::newRow("product enabled, module validation error")
            << "delayed-error/validation.qbs" << true;
    QTest::newRow("product disabled, module validation error")
            << "delayed-error/validation.qbs" << false;
    QTest::newRow("product enabled, module not found")
            << "delayed-error/nonexisting.qbs" << true;
    QTest::newRow("product disabled, module not found")
            << "delayed-error/nonexisting.qbs" << false;
}

void TestLanguage::dependencyOnAllProfiles()
{
    bool exceptionCaught = false;
    try {
        TemporaryProfile p1("p1", m_settings);
        p1.p.setValue("qbs.architecture", "arch1");
        TemporaryProfile p2("p2", m_settings);
        p2.p.setValue("qbs.architecture", "arch2");
        QVariantMap overriddenValues;
        overriddenValues.insert("project.profile1", "p1");
        overriddenValues.insert("project.profile2", "p2");
        defaultParameters.setOverriddenValues(overriddenValues);
        resolveProject("dependencyOnAllProfiles.qbs");
        QVERIFY(!!project);
        QCOMPARE(project->products.size(), size_t(3));
        const ResolvedProductConstPtr mainProduct = productsFromProject(project).value("main");
        QVERIFY(!!mainProduct);
        QCOMPARE(mainProduct->dependencies.size(), size_t { 2 });
        for (const ResolvedProductPtr &p : mainProduct->dependencies) {
            QCOMPARE(p->name, QLatin1String("dep"));
            QVERIFY(p->profile() == "p1" || p->profile() == "p2");
        }
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::dependsItemInGroup_data()
{
    QTest::addColumn<bool>("enableGroup1");
    QTest::addColumn<bool>("enableGroup2");
    QTest::addColumn<bool>("enableDepends");
    QTest::addColumn<bool>("dependencyExpected");

    QTest::newRow("all disabled") << false << false << false << false;
    QTest::newRow("only Depends enabled") << false << false << true << false;
    QTest::newRow("only inner Group enabled") << false << true << false << false;
    QTest::newRow("inner Group and Depends enabled") << false << true << true << false;
    QTest::newRow("only outer Group enabled") << true << false << false << false;
    QTest::newRow("only outer Group and Depends enabled") << true << false << true << false;
    QTest::newRow("only Groups enabled") << true << true << false << false;
    QTest::newRow("everything enabled") << true << true << true << true;
}

void TestLanguage::dependsItemInGroup()
{
    QFETCH(bool, enableGroup1);
    QFETCH(bool, enableGroup2);
    QFETCH(bool, enableDepends);
    QFETCH(bool, dependencyExpected);

    bool exceptionCaught = false;
    try {
        const QVariantMap overriddenValues{
            std::make_pair("products.main.enableGroup1", enableGroup1),
            std::make_pair("products.main.enableGroup2", enableGroup2),
            std::make_pair("products.main.enableDepends", enableDepends)};
        defaultParameters.setOverriddenValues(overriddenValues);
        resolveProject("depends-item-in-group.qbs");
        QVERIFY(project);
        const ResolvedProductConstPtr mainProduct = productsFromProject(project).value("main");
        QVERIFY(mainProduct);
        QCOMPARE(mainProduct->dependencies.empty(), !dependencyExpected);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::derivedSubProject()
{
    bool exceptionCaught = false;
    try {
        resolveProject("derived-sub-project/project.qbs");
        QVERIFY(!!project);
        const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 1);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::disabledPropertiesItem_data()
{
    QTest::addColumn<bool>("setInProduct");
    QTest::addColumn<bool>("setInHigher1");
    QTest::addColumn<bool>("setInHigher2");
    QTest::addColumn<bool>("setInLower");
    QTest::addColumn<QString>("expectedValue");
    QTest::addColumn<QStringList>("expectedListValue");
    QTest::addColumn<QStringList>("expectedProductValue");

    QTest::newRow("default")
        << false << false << false << false << QString("default")
        << QStringList{"N_GREATER_7", "N_GREATER_6", "N_GREATER_5", "N_LESS_20", "N_NON_ZERO"}
        << QStringList{"condition2", "default"};
    QTest::newRow("lower only")
        << false << false << false << true << QString("default_fromLower")
        << QStringList{"N_GREATER_7", "N_GREATER_6", "N_GREATER_5", "N_LESS_20", "WITH_LOWER_PROP", "N_NON_ZERO"}
        << QStringList{"condition2", "default"};
    QTest::newRow("higher2 only")
        << false << false << true << false << QString()
        << QStringList{"N_GREATER_7", "WITH_HIGHER2_PROP", "N_GREATER_6", "N_GREATER_5", "N_LESS_20", "N_NON_ZERO"}
        << QStringList{"condition2", "default"};
    QTest::newRow("lower and higher2")
        << false << false << true << true << QString()
        << QStringList{"N_GREATER_7", "WITH_HIGHER2_PROP", "N_GREATER_6", "N_GREATER_5", "N_LESS_20", "WITH_LOWER_PROP", "N_NON_ZERO"}
        << QStringList{"condition2", "default"};
    QTest::newRow("higher1 only")
        << false << true << false << false << QString("fromHigher1")
        << QStringList{"N_GREATER_7", "N_GREATER_6", "WITH_HIGHER1_PROP", "N_GREATER_5", "N_LESS_20", "N_NON_ZERO"}
        << QStringList{"condition2", "default"};
    QTest::newRow("lower and higher1")
        << false << true << false << true << QString("fromHigher1")
        << QStringList{"N_GREATER_7", "N_GREATER_6", "WITH_HIGHER1_PROP", "N_GREATER_5", "N_LESS_20", "WITH_LOWER_PROP", "N_NON_ZERO"}
        << QStringList{"condition2", "default"};
    QTest::newRow("product only")
        << true << false << false << false << QString("fromProduct")
        << QStringList{"WITH_PRODUCT_PROP", "N_GREATER_7", "N_GREATER_6", "N_GREATER_5", "N_LESS_20", "N_NON_ZERO"}
        << QStringList{"condition1", "condition2", "default"};
    QTest::newRow("product and lower")
        << true << false << false << true << QString("fromProduct")
        << QStringList{"WITH_PRODUCT_PROP", "N_GREATER_7", "N_GREATER_6", "N_GREATER_5", "N_LESS_20", "WITH_LOWER_PROP", "N_NON_ZERO"}
        << QStringList{"condition1", "condition2", "default"};
    QTest::newRow("product and higher2")
        << true << false << true << false << QString("fromProduct")
        << QStringList{"WITH_PRODUCT_PROP", "N_GREATER_7", "WITH_HIGHER2_PROP", "N_GREATER_6", "N_GREATER_5", "N_LESS_20", "N_NON_ZERO"}
        << QStringList{"condition1", "condition2", "default"};
    QTest::newRow("product, higher2 and lower")
        << true << false << true << true << QString("fromProduct")
        << QStringList{"WITH_PRODUCT_PROP", "N_GREATER_7", "WITH_HIGHER2_PROP", "N_GREATER_6", "N_GREATER_5", "N_LESS_20", "WITH_LOWER_PROP", "N_NON_ZERO"}
        << QStringList{"condition1", "condition2", "default"};
    QTest::newRow("product and higher1")
        << true << true << false << false << QString("fromProduct")
        << QStringList{"WITH_PRODUCT_PROP", "N_GREATER_7", "N_GREATER_6", "WITH_HIGHER1_PROP", "N_GREATER_5", "N_LESS_20", "N_NON_ZERO"}
        << QStringList{"condition1", "condition2", "default"};
    QTest::newRow("product, higher1 and higher2")
        << true << true << true << false << QString("fromProduct")
        << QStringList{"WITH_PRODUCT_PROP", "N_GREATER_7", "WITH_HIGHER2_PROP", "N_GREATER_6", "WITH_HIGHER1_PROP", "N_GREATER_5", "N_LESS_20", "N_NON_ZERO"}
        << QStringList{"condition1", "condition2", "default"};
    QTest::newRow("all")
        << true << true << true << true << QString("fromProduct")
        << QStringList{"WITH_PRODUCT_PROP", "N_GREATER_7", "WITH_HIGHER2_PROP", "N_GREATER_6", "WITH_HIGHER1_PROP", "N_GREATER_5", "N_LESS_20", "WITH_LOWER_PROP", "N_NON_ZERO"}
        << QStringList{"condition1", "condition2", "default"};
}

void TestLanguage::disabledPropertiesItem()
{
    QFETCH(bool, setInLower);
    QFETCH(bool, setInHigher1);
    QFETCH(bool, setInHigher2);
    QFETCH(bool, setInProduct);
    QFETCH(QString, expectedValue);
    QFETCH(QStringList, expectedListValue);
    QFETCH(QStringList, expectedProductValue);

    QVariantMap overriddenValues;
    overriddenValues.insert("modules.lower.setProp", setInLower);
    overriddenValues.insert("modules.higher1.setProp", setInHigher1);
    overriddenValues.insert("modules.higher2.setProp", setInHigher2);
    overriddenValues.insert("products.p.setProp", setInProduct);
    defaultParameters.setOverriddenValues(overriddenValues);

    bool exceptionCaught = false;
    try {
        resolveProject("disabled-properties-item/disabled-properties-item.qbs");
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);

    QVERIFY(!!project);
    const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
    QCOMPARE(products.size(), 1);
    const ResolvedProductConstPtr &p = *products.constBegin();
    QCOMPARE(p->moduleProperties->moduleProperty("lower", "prop").toString(), expectedValue);
    QCOMPARE(
        p->moduleProperties->moduleProperty("lower", "listProp").toStringList(), expectedListValue);
    QCOMPARE(p->productProperties.value("productProp").toStringList(), expectedProductValue);
}

void TestLanguage::disabledSubProject()
{
    bool exceptionCaught = false;
    try {
        resolveProject("disabled-subproject.qbs");
        QVERIFY(!!project);
        const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 0);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::dottedNames_data()
{
    QTest::addColumn<bool>("useProduct");
    QTest::addColumn<bool>("useModule");
    QTest::addColumn<bool>("expectSuccess");
    QTest::addColumn<QString>("expectedErrorMessage");
    QTest::newRow("missing product dependency") << false << true << false
            << QString("Item 'a.b' is not declared. Did you forget to add a Depends item");
    QTest::newRow("missing module dependency") << true << false << false
            << QString("Item 'x.y' is not declared. Did you forget to add a Depends item");
    QTest::newRow("missing both dependencies") << false << false << false << QString();
    QTest::newRow("ok") << true << true << true << QString();
}

void TestLanguage::dottedNames()
{
    QFETCH(bool, expectSuccess);
    try {
        QFETCH(bool, useProduct);
        QFETCH(bool, useModule);
        const QVariantMap overridden{
            std::make_pair("projects.theProject.includeDottedProduct", useProduct),
            std::make_pair("projects.theProject.includeDottedModule", useModule)
        };
        defaultParameters.setOverriddenValues(overridden);
        resolveProject("dotted-names/dotted-names.qbs");
        QVERIFY(expectSuccess);
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), useProduct ? 2 : 1);
        const ResolvedProductPtr product = products.value("p");
        QVERIFY(!!product);
        QCOMPARE(product->moduleProperties->moduleProperty("a.b", "c").toString(), QString("p"));
        QCOMPARE(product->moduleProperties->moduleProperty("x.y", "z").toString(), QString("p"));
    } catch (const ErrorInfo &e) {
        QVERIFY(!expectSuccess);
        QFETCH(QString, expectedErrorMessage);
        if (!expectedErrorMessage.isEmpty())
            QVERIFY2(e.toString().contains(expectedErrorMessage), qPrintable(e.toString()));
    }
}

void TestLanguage::duplicateMultiplexValues_data()
{
    QTest::addColumn<bool>("dummy");
    QTest::newRow("duplicate-multiplex-value") << true;
    QTest::newRow("duplicate-multiplex-value2") << true;
}

void TestLanguage::duplicateMultiplexValues()
{
    bool exceptionCaught = false;
    try {
        resolveProject(qPrintable(QString::fromLocal8Bit(QTest::currentDataTag())
                                  + QLatin1String(".qbs")));
        QVERIFY(project);
        const std::vector<ResolvedProductPtr> products = project->allProducts();
        QCOMPARE(products.size(), 2);
        bool x86 = false;
        bool arm = false;
        for (const ResolvedProductPtr &p : products) {
            if (p->moduleProperties->moduleProperty("qbs", "architecture").toString() == "x86")
                x86 = true;
            else if (p->moduleProperties->moduleProperty("qbs", "architecture").toString() == "arm")
                arm = true;
        }
        QVERIFY(x86);
        QVERIFY(arm);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QVERIFY(!exceptionCaught);
}

void TestLanguage::emptyJsFile()
{
    bool exceptionCaught = false;
    try {
        resolveProject("empty-js-file.qbs");
        QVERIFY(!!project);
        const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 1);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::enumerateProjectProperties()
{
    bool exceptionCaught = false;
    try {
        resolveProject("enum-project-props.qbs");
        QVERIFY(!!project);
        auto products = productsFromProject(project);
        QCOMPARE(products.size(), 1);
        auto product = products.values().front();
        auto files = product->groups.front()->files;
        QCOMPARE(product->groups.size(), size_t(1));
        QVERIFY(files);
        QCOMPARE(files->size(), size_t(1));
        auto fileName = FileInfo::fileName(files->front()->absoluteFilePath);
        QCOMPARE(fileName, QString("dummy.txt"));
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::evalErrorInNonPresentModule_data()
{
    QTest::addColumn<bool>("moduleRequired");
    QTest::addColumn<QString>("errorMessage");

    QTest::newRow("module required")
            << true << "broken.qbs:2:5 Element at index 0 of list property 'broken' "
                       "does not have string type";
    QTest::newRow("module not required") << false << QString();
}

void TestLanguage::evalErrorInNonPresentModule()
{
    QFETCH(bool, moduleRequired);
    QFETCH(QString, errorMessage);
    try {
        QVariantMap overridden{std::make_pair("products.p.moduleRequired", moduleRequired)};
        defaultParameters.setOverriddenValues(overridden);
        resolveProject("eval-error-in-non-present-module.qbs");
        QVERIFY(errorMessage.isEmpty());
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 1);
        const ResolvedProductPtr product = products.value("p");
        QVERIFY(!!product);
    } catch (const ErrorInfo &e) {
        QVERIFY(!errorMessage.isEmpty());
        QVERIFY2(e.toString().contains(errorMessage), qPrintable(e.toString()));
    }
}

void TestLanguage::defaultValue()
{
    bool exceptionCaught = false;
    try {
        QFETCH(QString, prop1Value);
        QVariantMap overridden;
        if (!prop1Value.isEmpty())
            overridden.insert("modules.lower.prop1", prop1Value);
        defaultParameters.setOverriddenValues(overridden);
        resolveProject("defaultvalue/egon.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 2);
        const ResolvedProductPtr product = products.value("egon");
        QVERIFY(!!product);
        QStringList propertyName = QStringList() << "lower" << "prop2";
        QVariant propertyValue = product->moduleProperties->property(propertyName);
        QFETCH(QVariant, expectedProp2Value);
        QCOMPARE(propertyValue, expectedProp2Value);
        propertyName = QStringList() << "lower" << "listProp";
        propertyValue = product->moduleProperties->property(propertyName);
        QFETCH(QVariant, expectedListPropValue);
        QCOMPARE(propertyValue.toStringList(), expectedListPropValue.toStringList());
    } catch (const ErrorInfo &e) {
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
            << QVariant(QStringList({"other"}));
    QTest::newRow("controlling property with blubb value") << "blubb" << QVariant("withBlubb")
            << QVariant(QStringList({"blubb", "other"}));
    QTest::newRow("controlling property with egon value") << "egon" << QVariant("withEgon")
            << QVariant(QStringList({"egon", "other"}));
    QTest::newRow("controlling property not overwritten") << "" << QVariant("withBlubb")
            << QVariant(QStringList({"blubb", "other"}));
}

void TestLanguage::environmentVariable()
{
    bool exceptionCaught = false;
    try {
        // Create new environment:
        const QString varName = QStringLiteral("PRODUCT_NAME");
        const QString productName = QLatin1String("MyApp") + QString::number(m_rand.generate());
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(varName, productName);

        QProcessEnvironment origEnv = defaultParameters.environment(); // store orig environment

        defaultParameters.setEnvironment(env);
        resolveProject("environmentvariable.qbs");

        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        ResolvedProductPtr product = products.value(productName);
        QVERIFY(!!product);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::errorInDisabledProduct()
{
    bool exceptionCaught = false;
    try {
        resolveProject("error-in-disabled-product.qbs");
        QVERIFY(!!project);
        auto products = productsFromProject(project);
        QCOMPARE(products.size(), 5);
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
            << "Dependency 'neitherModuleNorProduct' not found";
    QTest::newRow("multiple_exports")
            << "Multiple Export items in one product are prohibited.";
    QTest::newRow("multiple_properties_in_subproject")
            << "Multiple instances of item 'Properties' found where at most one "
               "is allowed.";
    QTest::newRow("importloop1")
            << "Loop detected when importing";
    QTest::newRow("nonexistentouter") << "outer is not defined";
    QTest::newRow("invalid_file") << "does not exist";
    QTest::newRow("invalid-parameter-rhs") << "access is not defined";
    QTest::newRow("invalid-parameter-type")
            << "Value assigned to property 'stringParameter' does not have type 'string'.";
    QTest::newRow("invalid_property_type")
            << "Unknown type 'nonsense' in property declaration.";
    QTest::newRow("reserved_name_in_import")
            << "Cannot reuse the name of built-in extension 'TextFile'.";
    QTest::newRow("throw_in_property_binding")
            << "something is wrong";
    QTest::newRow("no-configure-in-probe")
            << "no-configure-in-probe.qbs:2:5.*Probe.configure must be set";
    QTest::newRow("dependency_cycle")
            << "Cyclic dependencies detected.";
    QTest::newRow("dependency_cycle2")
            << "Cyclic dependencies detected.";
    QTest::newRow("dependency_cycle3")
            << "Cyclic dependencies detected.";
    QTest::newRow("dependency_cycle3a")
            << "Cyclic dependencies detected.";
    QTest::newRow("dependency_cycle4")
            << "Cyclic dependencies detected.";
    QTest::newRow("references_cycle")
            << "Cycle detected while referencing file '.*references_cycle.qbs'.";
    QTest::newRow("subproject_cycle")
            << "Cycle detected while loading subproject file 'subproject_cycle.qbs'.";
    QTest::newRow("invalid_stringlist_element")
            << "Element at index 1 of list property 'files' does not have string type.";
    QTest::newRow("undefined_stringlist_element")
            << "Element at index 1 of list property 'files' is undefined. String expected.";
    QTest::newRow("undefined_stringlist_element_in_probe")
            << "Element at index 1 of list property 'l' is undefined. String expected.";
    QTest::newRow("undeclared_item")
            << "Item 'cpp' is not declared.";
    QTest::newRow("undeclared-parameter1")
            << "Parameter 'prefix2.suffix.nope' is not declared.";
    QTest::newRow("undeclared-parameter2")
            << "Cannot set parameter 'foo.bar', "
               "because 'myproduct' does not have a dependency on 'foo'.";
    QTest::newRow("undeclared_property_wrapper")
            << "Property 'doesntexist' is not declared.";
    QTest::newRow("undeclared_property_in_export_item")
            << "Property 'blubb' is not declared.";
    QTest::newRow("undeclared_property_in_export_item2")
            << "Item 'something' is not declared.";
    QTest::newRow("undeclared_property_in_export_item3")
            << "Property 'blubb' is not declared.";
    QTest::newRow("undeclared_module_property_in_module")
            << "Property 'noSuchProperty' is not declared.";
    QTest::newRow("unknown_item_type")
            << "Unexpected item type 'Narf'";
    QTest::newRow("invalid_child_item_type")
            << "Items of type 'Project' cannot contain items of type 'Depends'.";
    QTest::newRow("conflicting_fileTagsFilter")
            << "Conflicting fileTagsFilter in Group items";
    QTest::newRow("duplicate_sources")
            << "Duplicate source file '.*main.cpp'"
               ".*duplicate_sources.qbs:2:12.*duplicate_sources.qbs:4:16.";
    QTest::newRow("duplicate_sources_wildcards")
            << "Duplicate source file '.*duplicate_sources_wildcards.qbs'"
               ".*duplicate_sources_wildcards.qbs:2:12"
               ".*duplicate_sources_wildcards.qbs:4:16.";
    QTest::newRow("oldQbsVersion")
            << "The project requires at least qbs version \\d+\\.\\d+.\\d+, "
               "but this is qbs version " QBS_VERSION ".";
    QTest::newRow("wrongQbsVersionFormat")
            << "The value '.*' of Project.minimumQbsVersion is not a valid version string.";
    QTest::newRow("properties-item-with-invalid-condition")
            << "properties-item-with-invalid-condition.qbs:4:19.*"
               "cannot read property 'includes' of undefined";
    QTest::newRow("misused-inherited-property") << "Binding to non-item property";
    QTest::newRow("undeclared_property_in_Properties_item") << "Item 'blubb' is not declared";
    QTest::newRow("conflicting-properties-in-export-items")
            << "Export item in inherited item redeclares property 'theProp' with different type.";
    QTest::newRow("invalid-property-option")
            << "PropertyOptions item refers to non-existing property 's0meProp'";
    QTest::newRow("missing-colon")
            << "Invalid item 'dummy.cxxFlags'. Did you mean to set a module property?";
    QTest::newRow("syntax-error-in-probe")
        << "syntax-error-in-probe.qbs:4:20.*fngkgsdjfgklkf is not defined";
    QTest::newRow("wrong-toplevel-item")
            << "wrong-toplevel-item.qbs:1:1.*The top-level item must be of type 'Project' or "
               "'Product', but it is of type 'Artifact'.";
    QTest::newRow("conflicting-module-instances")
            << "There is more than one equally prioritized candidate for module "
               "'conflicting-instances'.";
    QTest::newRow("overwrite-inherited-readonly-property")
            << "overwrite-inherited-readonly-property.qbs"
               ":2:21.*Cannot set read-only property 'readOnlyString'.";
    QTest::newRow("overwrite-readonly-module-property")
            << "overwrite-readonly-module-property.qbs"
               ":3:30.*Cannot set read-only property 'readOnlyString'.";
    QTest::newRow("original-in-product-property")
            << "original-in-product-property.qbs"
               ":2:21.*The special value 'original' can only be used with module properties.";
    QTest::newRow("rule-without-output-tags")
            << "rule-without-output-tags.qbs:2:5.*A rule needs to have Artifact items or "
               "a non-empty outputFileTags property.";
    QTest::newRow("original-in-module-prototype")
            << "module-with-invalid-original.qbs:2:24.*The special value 'original' cannot be used "
               "on the right-hand side of a property declaration.";
    QTest::newRow("original-in-export-item")
            << "original-in-export-item.qbs:5:32.*The special value 'original' cannot be used "
               "on the right-hand side of a property declaration.";
    QTest::newRow("original-in-export-item2")
            << "original-in-export-item2.qbs:4:9.*Item 'x.y' is not declared. Did you forget "
               "to add a Depends item";
    QTest::newRow("original-in-export-item3")
            << "original-in-export-item3.qbs:6:9.*Item 'x.y' is not declared. Did you forget "
               "to add a Depends item";
    QTest::newRow("mismatching-multiplex-dependency")
            << "mismatching-multiplex-dependency.qbs:9:9.*Dependency from product "
               "'b \\{\"architecture\":\"mips\"\\}' to product 'a'"
               " not fulfilled. There are no eligible multiplex candidates.";
    QTest::newRow("ambiguous-multiplex-dependency")
            << "ambiguous-multiplex-dependency.qbs:10:9.*Dependency from product 'b "
               "\\{\"architecture\":\"x86\"\\}' to product 'a' is ambiguous. Eligible multiplex "
               "candidates: a \\{\"architecture\":\"x86\",\"buildVariant\":\"debug\"\\}, "
               "a \\{\"architecture\":\"x86\",\"buildVariant\":\"release\"\\}.";
    QTest::newRow("dependency-profile-mismatch")
            << "dependency-profile-mismatch.qbs:10:5.*Product 'main' depends on 'dep', "
               "which does not exist for the requested profile 'profile47'.";
    QTest::newRow("dependency-profile-mismatch-2")
            << "dependency-profile-mismatch-2.qbs:15:9 Dependency from product 'main' to "
               "product 'dep' not fulfilled. There are no eligible multiplex candidates.";
    QTest::newRow("invalid-references")
            << "invalid-references.qbs:2:17.*Cannot open '.*nosuchproject.qbs'";
    QTest::newRow("missing-js-file")
        << "missing-js-file.qbs.*Cannot open '.*javascriptfile.js'.*missing-js-file.js:1";
    QTest::newRow("frozen-object") << "'key' is read-only";
    QTest::newRow("frozen-object-list") << "object is not extensible";
    QTest::newRow("module-property-binding-in-project")
        << "Module properties cannot be set in Project items";
    QTest::newRow("module-with-id") << "Module items cannot have an id property";
    QTest::newRow("module-var-in-product")
        << "module-var-in-product.qbs:4:19 Use of 'module' is only allowed in Module items";
}

void TestLanguage::erroneousFiles()
{
    QFETCH(QString, errorMessage);
    QString fileName = QString::fromLocal8Bit(QTest::currentDataTag()) + QLatin1String(".qbs");
    try {
        resolveProject(qPrintable("/erroneous/" + fileName));
    } catch (const ErrorInfo &e) {
        const QRegularExpression reg(errorMessage, QRegularExpression::DotMatchesEverythingOption);
        if (!e.toString().contains(reg)) {
            qDebug() << "Message:  " << e.toString();
            qDebug() << "Expected: " << errorMessage;
            QFAIL("Unexpected error message.");
        }
        return;
    }
    QEXPECT_FAIL("undeclared_property_in_Properties_item", "Too expensive to check", Continue);
    QEXPECT_FAIL("original-in-export-item3", "Too expensive to check", Continue);
    QVERIFY(!"No error thrown on invalid input.");
}

void TestLanguage::exports()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setDeprecationWarningMode(DeprecationWarningMode::On);
        resolveProject("exports.qbs");
        QVERIFY(!!project);
        Set<CodeLocation> warningLocations;
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 22);
        ResolvedProductPtr product;
        product = products.value("myapp");
        QVERIFY(!!product);
        QStringList propertyName = QStringList() << "dummy" << "defines";
        QVariant propertyValue = product->moduleProperties->property(propertyName);
        QCOMPARE(propertyValue.toStringList(), QStringList() << "BUILD_MYAPP" << "USE_MYLIB"
                 << "USE_MYLIB2");
        propertyName = QStringList() << "dummy" << "includePaths";
        QVariantList propertyValues = product->moduleProperties->property(propertyName).toList();
        QCOMPARE(propertyValues.size(), 3);
        QVERIFY(propertyValues.at(0).toString().endsWith("/app"));
        QVERIFY(propertyValues.at(1).toString().endsWith("/testdata/lib"));
        QVERIFY(propertyValues.at(2).toString().endsWith("/testdata/lib"));

        QCOMPARE(product->moduleProperties->moduleProperty("dummy", "productName").toString(),
                 QString("myapp"));
        QVERIFY(product->moduleProperties->moduleProperty("dummy", "somePath").toString()
                .endsWith("/subdir"));

        product = products.value("mylib");
        QVERIFY(!!product);
        propertyName = QStringList() << "dummy" << "defines";
        propertyValue = product->moduleProperties->property(propertyName);
        QCOMPARE(propertyValue.toStringList(), QStringList() << "BUILD_MYLIB");
        QVERIFY(product->moduleProperties->moduleProperty("dummy", "somePath").toString()
                .endsWith("/subdir"));

        product = products.value("mylib2");
        QVERIFY(!!product);
        propertyName = QStringList() << "dummy" << "defines";
        propertyValue = product->moduleProperties->property(propertyName);
        QCOMPARE(propertyValue.toStringList(), QStringList() << "BUILD_MYLIB2");

        product = products.value("A");
        QVERIFY(!!product);
        QVERIFY(contains(product->dependencies, products.value("B")));
        QVERIFY(contains(product->dependencies, products.value("C")));
        QVERIFY(contains(product->dependencies, products.value("D")));
        product = products.value("B");
        QVERIFY(!!product);
        QVERIFY(product->dependencies.empty());
        QCOMPARE(product->exportedModule.productDependencies, std::vector<QString>{"C"});
        product = products.value("C");
        QVERIFY(!!product);
        QVERIFY(product->dependencies.empty());
        product = products.value("D");
        QVERIFY(!!product);
        QVERIFY(product->dependencies.empty());

        product = products.value("myapp2");
        QVERIFY(!!product);
        propertyName = QStringList() << "dummy" << "cFlags";
        propertyValue = product->moduleProperties->property(propertyName);
        QCOMPARE(propertyValue.toStringList(), QStringList()
                 << "BASE_PRODUCTWITHINHERITEDEXPORTITEM"
                 << "PRODUCT_PRODUCTWITHINHERITEDEXPORTITEM");
        propertyName = QStringList() << "dummy" << "cxxFlags";
        propertyValue = product->moduleProperties->property(propertyName);
        QCOMPARE(propertyValue.toStringList(), QStringList() << "-bar");
        propertyName = QStringList() << "dummy" << "defines";
        propertyValue = product->moduleProperties->property(propertyName);
        QCOMPARE(propertyValue.toStringList(), QStringList({"ABC", "DEF"}));
        QCOMPARE(product->moduleProperties->moduleProperty("dummy", "productName").toString(),
                 QString("myapp2"));
        QCOMPARE(product->moduleProperties->moduleProperty("dummy",
                "upperCaseProductName").toString(), QString("MYAPP2"));

        // Check whether we're returning incorrect cached values.
        product = products.value("myapp3");
        QVERIFY(!!product);
        QCOMPARE(product->moduleProperties->moduleProperty("dummy", "productName").toString(),
                 QString("myapp3"));
        QCOMPARE(product->moduleProperties->moduleProperty("dummy",
                "upperCaseProductName").toString(), QString("MYAPP3"));

        // Verify we refer to the right "project" variable.
        product = products.value("sub p2");
        QVERIFY(!!product);
        QCOMPARE(product->moduleProperties->moduleProperty("dummy", "someString").toString(),
                 QString("sub1"));

        product = products.value("libE");
        QVERIFY(!!product);
        propertyName = QStringList() << "dummy" << "defines";
        propertyValue = product->moduleProperties->property(propertyName);
        QCOMPARE(propertyValue.toStringList(),
                 QStringList() << "LIBD" << "LIBC" << "LIBA" << "LIBB");
        propertyName = QStringList() << "dummy" << "productName";
        propertyValue = product->moduleProperties->property(propertyName);
        QCOMPARE(propertyValue.toString(), QString("libE"));

        product = products.value("depender");
        QVERIFY(!!product);
        QCOMPARE(product->modules.size(), size_t(2));
        for (const ResolvedModulePtr &m : product->modules) {
            QVERIFY2(m->name == QString("qbs") || m->name == QString("dependency"),
                     qPrintable(m->name));
        }
        QCOMPARE(product->productProperties.value("featureX").toBool(), true);
        QCOMPARE(product->productProperties.value("featureY").toBool(), false);
        QCOMPARE(product->productProperties.value("featureZ").toBool(), true);

        product = products.value("broken_cycle3");
        QVERIFY(!!product);
        QCOMPARE(product->modules.size(), size_t(3));
        for (const ResolvedModulePtr &m : product->modules) {
            QVERIFY2(m->name == QString("qbs") || m->name == QString("broken_cycle1")
                     || m->name == QString("broken_cycle2"),
                     qPrintable(m->name));
        }
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::fileContextProperties()
{
    bool exceptionCaught = false;
    try {
        resolveProject("filecontextproperties.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        ResolvedProductPtr product = products.value("product1");
        QVERIFY(!!product);
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

void TestLanguage::fileInProductAndModule_data()
{
    QTest::addColumn<bool>("file1IsTarget");
    QTest::addColumn<bool>("file2IsTarget");
    QTest::addColumn<bool>("addFileToProduct");
    QTest::addColumn<bool>("successExpected");
    QTest::newRow("file occurs twice in module as non-target") << false << false << false << false;
    QTest::newRow("file occurs twice in module as non-target, and in product as well")
            << false << false << true << false;
    QTest::newRow("file occurs in module as non-target and target")
            << false << true << false << true;
    QTest::newRow("file occurs in module as non-target and target, and in product as well")
            << false << true << true << false;
    QTest::newRow("file occurs in module as target and non-target")
            << true << false << false << true;
    QTest::newRow("file occurs in module as target and non-target, and in product as well")
            << true << false << true << false;
    QTest::newRow("file occurs twice in module as target") << true << true << false << false;
    QTest::newRow("file occurs twice in module as target, and in product as well")
            << true << true << true << false;
}

void TestLanguage::fileInProductAndModule()
{
    bool exceptionCaught = false;
    QFETCH(bool, file1IsTarget);
    QFETCH(bool, file2IsTarget);
    QFETCH(bool, addFileToProduct);
    QFETCH(bool, successExpected);
    try {
        defaultParameters.setOverriddenValues(QVariantMap{
            std::make_pair("modules.module_with_file.file1IsTarget", file1IsTarget),
            std::make_pair("modules.module_with_file.file2IsTarget", file2IsTarget),
            std::make_pair("products.p.addFileToProduct", addFileToProduct),
        });
        resolveProject("file-in-product-and-module.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 1);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        QVERIFY2(e.toString().contains("Duplicate"), qPrintable(e.toString()));
    }
    QCOMPARE(exceptionCaught, !successExpected);
}

void TestLanguage::getNativeSetting()
{
    bool exceptionCaught = false;
    try {
        resolveProject("getNativeSetting.qbs");

        QString expectedTargetName;
        if (HostOsInfo::isMacosHost()) {
            if (HostOsInfo::hostOsVersion() >= qbs::Version(11))
                expectedTargetName = QStringLiteral("macOS");
            else
                expectedTargetName = QStringLiteral("Mac OS X");
        } else if (HostOsInfo::isWindowsHost()) {
            expectedTargetName = QStringLiteral("Windows");
        } else {
            expectedTargetName = QStringLiteral("Unix");
        }

        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products;
        for (const ResolvedProductPtr &product : project->allProducts())
            products.insert(product->targetName, product);
        ResolvedProductPtr product = products.value(expectedTargetName);
        QVERIFY(!!product);
        ResolvedProductPtr product2 = products.value(QStringLiteral("fallback"));
        QVERIFY(!!product2);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::groupConditions_data()
{
    QTest::addColumn<size_t>("groupCount");
    QTest::addColumn<std::vector<bool>>("groupEnabled");
    QTest::newRow("init") << size_t(0) << std::vector<bool>();
    QTest::newRow("no_condition_no_group")
            << size_t(1) << std::vector<bool>{ true };
    QTest::newRow("no_condition")
            << size_t(2) << std::vector<bool>{ true, true };
    QTest::newRow("true_condition")
            << size_t(2) << std::vector<bool>{ true, true };
    QTest::newRow("false_condition")
            << size_t(2) << std::vector<bool>{ true, false };
    QTest::newRow("true_condition_from_product")
            << size_t(2) << std::vector<bool>{ true, true };
    QTest::newRow("true_condition_from_project")
            << size_t(2) << std::vector<bool>{ true, true };
    QTest::newRow("condition_accessing_module_property")
            << size_t(2) << std::vector<bool>{ true, false };
    QTest::newRow("cleanup") << size_t(0) << std::vector<bool>();
}

void TestLanguage::groupConditions()
{
    HANDLE_INIT_CLEANUP_DATATAGS("groupconditions.qbs");
    QFETCH(size_t, groupCount);
    QFETCH(std::vector<bool>, groupEnabled);
    QCOMPARE(groupCount, groupEnabled.size());
    const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
    const QString productName = QString::fromLocal8Bit(QTest::currentDataTag());
    ResolvedProductPtr product = products.value(productName);
    QVERIFY(!!product);
    QCOMPARE(product->name, productName);
    QCOMPARE(product->groups.size(), groupCount);
    for (size_t i = 0; i < groupCount; ++i) {
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
        resolveProject("groupname.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 2);

        ResolvedProductPtr product = products.value("MyProduct");
        QVERIFY(!!product);
        QCOMPARE(product->groups.size(), size_t(2));
        GroupConstPtr group = product->groups.at(0);
        QVERIFY(!!group);
        QCOMPARE(group->name, QString("MyProduct"));
        group = product->groups.at(1);
        QVERIFY(!!group);
        QCOMPARE(group->name, QString("MyProduct.MyGroup"));

        product = products.value("My2ndProduct");
        QVERIFY(!!product);
        QCOMPARE(product->groups.size(), size_t(3));
        group = product->groups.at(0);
        QVERIFY(!!group);
        QCOMPARE(group->name, QString("My2ndProduct"));
        group = product->groups.at(1);
        QVERIFY(!!group);
        QCOMPARE(group->name, QString("My2ndProduct.MyGroup"));
        group = product->groups.at(2);
        QVERIFY(!!group);
        QCOMPARE(group->name, QString("Group 2"));
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::homeDirectory()
{
    try {
        resolveProject("homeDirectory.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 1);

        ResolvedProductPtr product = products.value("home");
        QVERIFY(!!product);

        QDir dir = QDir::home();
        QCOMPARE(product->productProperties.value("home").toString(), dir.path());
        QCOMPARE(product->productProperties.value("homeSlash").toString(), dir.path());

        dir.cdUp();
        QCOMPARE(product->productProperties.value("homeUp").toString(), dir.path());

        dir = QDir::home();
        QCOMPARE(product->productProperties.value("homeFile").toString(),
                 dir.filePath("a"));

        QCOMPARE(product->productProperties.value("bogus1").toString(),
                 FileInfo::resolvePath(product->sourceDirectory, QStringLiteral("a~b")));
        QCOMPARE(product->productProperties.value("bogus2").toString(),
                 FileInfo::resolvePath(product->sourceDirectory, QStringLiteral("a/~/bb")));
        QCOMPARE(product->productProperties.value("user").toString(),
                 FileInfo::resolvePath(product->sourceDirectory, QStringLiteral("~foo/bar")));
    } catch (const ErrorInfo &e) {
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
                                  "        console.info(foo);\n"
                                  "        return foo;\n"
                                  "    }\n"
                                  "}\n");
    QTest::newRow("narf, no zort") << true << false << QString(
                                  "Product {\n"
                                  "    name: {\n"
                                  "        var foo = 'zort';\n"
                                  "        console.info(narf + foo);\n"
                                  "        return foo;\n"
                                  "    }\n"
                                  "}\n");
    QTest::newRow("no narf, zort") << false << true << QString(
                                  "Product {\n"
                                  "    name: {\n"
                                  "        var foo = 'narf';\n"
                                  "        console.info(zort + foo);\n"
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
        resolveProject("idusage.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 5);
        QVERIFY(products.contains("product1_1"));
        QVERIFY(products.contains("product2_2"));
        QVERIFY(products.contains("product3_3"));
        ResolvedProductPtr product4 = products.value("product4_4");
        QVERIFY(!!product4);
        auto product4Property = [&product4] (const char *name) {
            return product4->productProperties.value(QString::fromUtf8(name)).toString();
        };
        QCOMPARE(product4Property("productName"), product4->name);
        QCOMPARE(product4Property("productNameInBaseOfBase"), product4->name);
        GroupPtr group = findByName(product4->groups, "group in base product");
        QVERIFY(!!group);
        QCOMPARE(qPrintable(group->prefix), "group in base product");
        group = findByName(product4->groups, "another group in base product");
        QVERIFY(!!group);
        QCOMPARE(qPrintable(group->prefix), "another group in base product");
        ResolvedProductPtr product5 = products.value("product5");
        QVERIFY(!!product5);
        QCOMPARE(product5->moduleProperties->moduleProperty("deepdummy.deep.moat", "zort")
                 .toString(), QString("zort in dummy"));
        QCOMPARE(int(product5->allEnabledFiles().size()), 1);
        QVERIFY(product5->allEnabledFiles().at(0)->absoluteFilePath.endsWith("dummy.txt"));
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QVERIFY(!exceptionCaught);
}

void TestLanguage::idUniqueness()
{
    bool exceptionCaught = false;
    try {
        resolveProject("id-uniqueness.qbs");
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        const QList<ErrorItem> items = e.items();
        QCOMPARE(items.size(), 3);
        QCOMPARE(items.at(0).toString(), QString::fromUtf8("The id 'baseProduct' is not unique."));
        QVERIFY(items.at(1).toString().contains("id-uniqueness.qbs:5:5 First occurrence is here."));
        QVERIFY(items.at(2).toString().contains("id-uniqueness.qbs:8:5 Next occurrence is here."));
    }
    QVERIFY(exceptionCaught);
}

void TestLanguage::importCollection()
{
    bool exceptionCaught = false;
    try {
        resolveProject("import-collection/project.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        const ResolvedProductConstPtr product = products.value("da product");
        QCOMPARE(product->productProperties.value("targetName").toString(),
                 QLatin1String("C1f1C1f2C2f1C2f2"));
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QVERIFY(!exceptionCaught);
}

void TestLanguage::inheritedPropertiesItems_data()
{
    QTest::addColumn<QString>("buildVariant");
    QTest::addColumn<QString>("productName");
    QTest::newRow("debug build") << "debug" << "product_debug";
    QTest::newRow("release build") << "release" << "product_release";
}

void TestLanguage::inheritedPropertiesItems()
{
    bool exceptionCaught = false;
    try {
        QFETCH(QString, buildVariant);
        QFETCH(QString, productName);
        defaultParameters.setOverriddenValues(QVariantMap{std::make_pair("qbs.buildVariant", buildVariant)});
        resolveProject("inherited-properties-items/inherited-properties-items.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 1);
        QVERIFY(!!products.value(productName));
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QVERIFY(!exceptionCaught);
}

void TestLanguage::invalidBindingInDisabledItem()
{
    bool exceptionCaught = false;
    try {
        resolveProject("invalidBindingInDisabledItem.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 2);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QVERIFY(!exceptionCaught);
}

void TestLanguage::invalidOverrides()
{
    QFETCH(QString, key);
    QFETCH(QString, expectedErrorMessage);
    const bool successExpected = expectedErrorMessage.isEmpty();
    bool exceptionCaught = false;
    try {
        defaultParameters.setOverriddenValues(QVariantMap{std::make_pair(key, true)});
        resolveProject("invalid-overrides.qbs");
        QVERIFY(!!project);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        if (successExpected)
            qDebug() << e.toString();
        else
            QVERIFY2(e.toString().contains(expectedErrorMessage), qPrintable(e.toString()));
    }
    QEXPECT_FAIL("no such module in product", "not easily checkable", Continue);
    QCOMPARE(!exceptionCaught, successExpected);
}

void TestLanguage::invalidOverrides_data()
{
    QTest::addColumn<QString>("key");
    QTest::addColumn<QString>("expectedErrorMessage");

    QTest::newRow("no such project") << "projects.myproject.x"
            << QString("Unknown project 'myproject' in property override.");
    QTest::newRow("no such project property") << "projects.My.Project.y"
                                              << QString("Unknown property: projects.My.Project.y");
    QTest::newRow("valid project property override") << "projects.My.Project.x" << QString();
    QTest::newRow("no such product") << "products.myproduct.x"
            << QString("Unknown product 'myproduct' in property override.");
    QTest::newRow("no such product (with module)") << "products.myproduct.cpp.useRPaths"
            << QString("Unknown product 'myproduct' in property override.");
    QTest::newRow("no such product property") << "products.MyProduct.y"
                                              << QString("Unknown property: products.MyProduct.y");
    QTest::newRow("valid product property override") << "products.MyProduct.x" << QString();

    // This cannot be an error, because the semantics are "if some product in the project has
    // such a module, then set that property", and the code that does the property overrides
    // does not have a global view.
    QTest::newRow("no such module") << "modules.blubb.x" << QString();

    QTest::newRow("no such module in product") << "products.MyProduct.cpp.useRPaths"
            << QString("Invalid module 'cpp' in property override.");
    QTest::newRow("no such module property") << "modules.cpp.blubb"
                                             << QString("Unknown property: modules.cpp.blubb");
    QTest::newRow("no such module property (per product)") << "products.MyOtherProduct.cpp.blubb"
            << QString("Unknown property: products.MyOtherProduct.cpp.blubb");
    QTest::newRow("valid per-product module property override")
            << "products.MyOtherProduct.cpp.useRPaths" << QString();
}

void TestLanguage::invalidPropOnNonRequiredModule_data()
{
    QTest::addColumn<bool>("useExistingModule");
    QTest::addColumn<bool>("errorExpected");

    QTest::newRow("existing module") << true << true;
    QTest::newRow("non-existing module") << false << false;
}

void TestLanguage::invalidPropOnNonRequiredModule()
{
    QFETCH(bool, useExistingModule);
    QFETCH(bool, errorExpected);

    try {
        defaultParameters.setOverriddenValues(
                    {std::make_pair("project.useExistingModule", useExistingModule)});
        resolveProject("invalid-prop-on-non-required-module");
        QVERIFY(!errorExpected);
    } catch (const ErrorInfo &e) {
        const QString errorString = e.toString();
        QVERIFY2(errorExpected, qPrintable(errorString));
        QVERIFY2(errorString.contains("Property 'nosuchprop' is not declared"),
                 qPrintable(errorString));
    }
}

void TestLanguage::itemPrototype()
{
    FileContextPtr fileContext = FileContext::create();
    fileContext->setFilePath("/dev/null");
    JSSourceValueCreator sourceValueCreator(fileContext);
    ItemPool pool;
    Item *proto = Item::create(&pool, ItemType::Product);
    proto->setProperty("x", sourceValueCreator.create("1"));
    proto->setProperty("y", sourceValueCreator.create("1"));
    Item *item = Item::create(&pool, ItemType::Product);
    item->setPrototype(proto);
    item->setProperty("y", sourceValueCreator.create("x + 1"));
    item->setProperty("z", sourceValueCreator.create("2"));

    Evaluator evaluator(m_engine.get());
    JSContext * const ctx = m_engine->context();
    QCOMPARE(getJsVariant(ctx, evaluator.property(proto, "x")).toInt(), 1);
    QCOMPARE(getJsVariant(ctx, evaluator.property(proto, "y")).toInt(), 1);
    QVERIFY(JS_IsUndefined(evaluator.property(proto, "z")));
    QCOMPARE(getJsVariant(ctx, evaluator.property(item, "x")).toInt(), 1);
    QCOMPARE(getJsVariant(ctx, evaluator.property(item, "y")).toInt(), 2);
    QCOMPARE(getJsVariant(ctx, evaluator.property(item, "z")).toInt(), 2);
}

void TestLanguage::itemScope()
{
    FileContextPtr fileContext = FileContext::create();
    fileContext->setFilePath("/dev/null");
    JSSourceValueCreator sourceValueCreator(fileContext);
    ItemPool pool;
    Item *scope1 = Item::create(&pool, ItemType::Scope);
    scope1->setProperty("x", sourceValueCreator.create("1"));
    Item *scope2 = Item::create(&pool, ItemType::Scope);
    scope2->setScope(scope1);
    scope2->setProperty("y", sourceValueCreator.create("x + 1"));
    Item *item = Item::create(&pool, ItemType::Scope);
    item->setScope(scope2);
    item->setProperty("z", sourceValueCreator.create("x + y"));

    Evaluator evaluator(m_engine.get());
    JSContext * const ctx = m_engine->context();
    QCOMPARE(getJsVariant(ctx, evaluator.property(scope1, "x")).toInt(), 1);
    QCOMPARE(getJsVariant(ctx, evaluator.property(scope2, "y")).toInt(), 2);
    QVERIFY(JS_IsUndefined(evaluator.property(scope2, "x")));
    QCOMPARE(getJsVariant(ctx, evaluator.property(item, "z")).toInt(), 3);
}

void TestLanguage::jsExtensions()
{
    QFile file(testProject("jsextensions.js"));
    QVERIFY(file.open(QFile::ReadOnly));
    QTextStream ts(&file);
    QString code = ts.readAll();
    QVERIFY(!code.isEmpty());
    m_engine->evaluate(JsValueOwner::Caller, code, file.fileName(), 1);
    if (m_engine->checkForJsError({})) {
        const ErrorInfo ex = m_engine->getAndClearJsError();
        QFAIL(qPrintable(ex.toString()));
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
        defaultParameters.setOverriddenValues({std::make_pair(QStringLiteral("qbs.buildVariant"),
                                                              buildVariant)});
        resolveProject("jsimportsinmultiplescopes.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 1);
        ResolvedProductPtr product = products.values().front();
        QVERIFY(!!product);
        QCOMPARE(product->name, expectedProductName);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QVERIFY(!exceptionCaught);
}

void TestLanguage::keepLoadingDependencies()
{
    QString error;
    try {
        resolveProject("keep-loading-dependencies.qbs");
        QFAIL("Should not get here!");
    } catch (const ErrorInfo &e) {
        error = e.toString();
    }
    QVERIFY2(error.contains("Dependency 'none.m1' not found"), qPrintable(error));
    QVERIFY2(error.contains("Dependency 'none.m2' not found"), qPrintable(error));
    QVERIFY2(error.contains("Dependency 'none.m3' not found"), qPrintable(error));
    QVERIFY2(!error.contains("QBS_CHECK"), qPrintable(error));
}

void TestLanguage::localProfileAsTopLevelProfile()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setTopLevelProfile("test-profile");
        resolveProject("local-profile-as-top-level-profile.qbs");
        QVERIFY(!!project);
        QCOMPARE(int(project->products.size()), 1);
        const PropertyMapConstPtr &props = project->products.front()->moduleProperties;
        QCOMPARE(props->qbsPropertyValue("architecture"), "arm");
        QCOMPARE(props->qbsPropertyValue("targetPlatform"), "macos");
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::moduleMergingVariantValues()
{
    bool exceptionCaught = false;
    try {
        resolveProject("module-merging-variant-values/module-merging-variant-values.qbs");
        QVERIFY(!!project);
        QCOMPARE(int(project->products.size()), 2);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::moduleNameCollisions_data()
{
    QTest::addColumn<QString>("projectFile");
    QTest::addColumn<bool>("collisionExpected");

    QTest::newRow("simple collision (one order)") << "simple-collision1.qbs" << true;
    QTest::newRow("simple collision (other order)") << "simple-collision2.qbs" << true;
    QTest::newRow("collision with more components") << "complex-collision.qbs" << true;
    QTest::newRow("no collision (same length)") << "no-collision1.qbs" << false;
    QTest::newRow("no collision (different length)") << "no-collision2.qbs" << false;
}

void TestLanguage::moduleNameCollisions()
{
    QFETCH(QString, projectFile);
    QFETCH(bool, collisionExpected);

    try {
        const QString compositeProjectFilePath = QString("module-name-collisions/") + projectFile;
        QVERIFY(resolveProject(qPrintable(compositeProjectFilePath)));
        QVERIFY(!collisionExpected);
    } catch (const ErrorInfo &e) {
        const QString errorString = e.toString();
        QVERIFY2(collisionExpected, qPrintable(errorString));
        QVERIFY2(errorString.contains("not allowed"), qPrintable(errorString));
    }
}

void TestLanguage::moduleParameters_data()
{
    QTest::addColumn<QVariantMap>("inputProperties");
    QTest::addColumn<QVariantMap>("expectedModuleParameters");
    QTest::addColumn<bool>("errorExpected");

    QTest::newRow("no overrides")
            << QVariantMap{
               std::make_pair("project.overrideFromModule", "false"),
               std::make_pair("project.overrideFromExport", "false"),
               std::make_pair("project.overrideFromProduct", "false")}
            << QVariantMap{
               std::make_pair("higher",
                              QVariantMap{std::make_pair("lower",
                                          QVariantMap{std::make_pair("param", "fromParameters")})}),
               std::make_pair("dep",
                              QVariantMap{std::make_pair("lower",
                                          QVariantMap{std::make_pair("param", "fromParameters")})})}
            << false;
    QTest::newRow("override from product")
            << QVariantMap{
               std::make_pair("project.overrideFromModule", "false"),
               std::make_pair("project.overrideFromExport", "false"),
               std::make_pair("project.overrideFromProduct", "true")}
            << QVariantMap{
               std::make_pair("higher",
                              QVariantMap{std::make_pair("lower",
                                          QVariantMap{std::make_pair("param", "fromProductDepends")})}),
               std::make_pair("dep",
                              QVariantMap{std::make_pair("lower",
                                          QVariantMap{std::make_pair("param", "fromProductDepends")})})}
            << false;
    QTest::newRow("override from export")
            << QVariantMap{
               std::make_pair("project.overrideFromModule", "false"),
               std::make_pair("project.overrideFromExport", "true"),
               std::make_pair("project.overrideFromProduct", "false")}
            << QVariantMap{
               std::make_pair("higher",
                              QVariantMap{std::make_pair("lower",
                                          QVariantMap{std::make_pair("param", "fromExportDepends")})}),
               std::make_pair("dep",
                              QVariantMap{std::make_pair("lower",
                                          QVariantMap{std::make_pair("param", "fromParameters")})})}
            << false;
    QTest::newRow("override from export and product")
            << QVariantMap{
               std::make_pair("project.overrideFromModule", "false"),
               std::make_pair("project.overrideFromExport", "true"),
               std::make_pair("project.overrideFromProduct", "true")}
            << QVariantMap{
               std::make_pair("higher",
                              QVariantMap{std::make_pair("lower",
                                          QVariantMap{std::make_pair("param", "fromProductDepends")})}),
               std::make_pair("dep",
                              QVariantMap{std::make_pair("lower",
                                          QVariantMap{std::make_pair("param", "fromProductDepends")})})}
            << false;
    QTest::newRow("override from module")
            << QVariantMap{
               std::make_pair("project.overrideFromModule", "true"),
               std::make_pair("project.overrideFromExport", "false"),
               std::make_pair("project.overrideFromProduct", "false")}
            << QVariantMap{
               std::make_pair("higher",
                              QVariantMap{std::make_pair("lower",
                                          QVariantMap{std::make_pair("param", "fromModuleDepends")})}),
               std::make_pair("dep",
                              QVariantMap{std::make_pair("lower",
                                          QVariantMap{std::make_pair("param", "fromParameters")})})}
            << false;
    QTest::newRow("override from module and product")
            << QVariantMap{
               std::make_pair("project.overrideFromModule", "true"),
               std::make_pair("project.overrideFromExport", "false"),
               std::make_pair("project.overrideFromProduct", "true")}
            << QVariantMap{
               std::make_pair("higher",
                              QVariantMap{std::make_pair("lower",
                                          QVariantMap{std::make_pair("param", "fromProductDepends")})}),
               std::make_pair("dep",
                              QVariantMap{std::make_pair("lower",
                                          QVariantMap{std::make_pair("param", "fromProductDepends")})})}
            << false;
    QTest::newRow("override from module and export")
            << QVariantMap{
               std::make_pair("project.overrideFromModule", "true"),
               std::make_pair("project.overrideFromExport", "true"),
               std::make_pair("project.overrideFromProduct", "false")}
            << QVariantMap{
               std::make_pair("higher",
                              QVariantMap{std::make_pair("lower",
                                          QVariantMap{std::make_pair("param", "fromExportDepends")})}),
               std::make_pair("dep",
                              QVariantMap{std::make_pair("lower",
                                          QVariantMap{std::make_pair("param", "fromParameters")})})}
            << true;
    QTest::newRow("override from module, export and product")
            << QVariantMap{
               std::make_pair("project.overrideFromModule", "true"),
               std::make_pair("project.overrideFromExport", "true"),
               std::make_pair("project.overrideFromProduct", "true")}
            << QVariantMap{
               std::make_pair("higher",
                              QVariantMap{std::make_pair("lower",
                                          QVariantMap{std::make_pair("param", "fromProductDepends")})}),
               std::make_pair("dep",
                              QVariantMap{std::make_pair("lower",
                                          QVariantMap{std::make_pair("param", "fromProductDepends")})})}
            << false;
}

void TestLanguage::moduleParameters()
{
    QFETCH(QVariantMap, inputProperties);
    QFETCH(QVariantMap, expectedModuleParameters);
    QFETCH(bool, errorExpected);

    try {
        defaultParameters.setOverriddenValues(inputProperties);
        resolveProject("module-parameters/module-parameters.qbs");
        QVERIFY(!errorExpected);
        QVERIFY(project);
        QCOMPARE(int(project->products.size()), 2);
        const ResolvedProductPtr mainProduct = productsFromProject(project).value("main");
        QVERIFY(mainProduct);
        QCOMPARE(int(mainProduct->moduleParameters.size()), 2);
        for (auto it = expectedModuleParameters.cbegin(); it != expectedModuleParameters.cend();
             ++it) {
            const auto findInProduct = [&](const QString &moduleName) {
                for (auto it = mainProduct->moduleParameters.cbegin();
                     it != mainProduct->moduleParameters.cend(); ++it) {
                    if (it.key()->name == moduleName)
                        return it.value();
                }
                return QVariantMap();
            };
            const QVariantMap actual = findInProduct(it.key());
            const QVariantMap expected = it.value().toMap();
            const bool same = qVariantMapsEqual(actual, expected);
            if (!same) {
                qDebug().noquote() << "---" << expected;
                qDebug().noquote() << "+++" << actual;
            }
            QVERIFY(same);
        }
    } catch (const ErrorInfo &e) {
        QVERIFY2(errorExpected, qPrintable(e.toString()));
    }
}

void TestLanguage::modulePrioritizationBySearchPath_data()
{
    QTest::addColumn<QStringList>("searchPaths");
    QTest::addColumn<QString>("expectedVariant");
    QTest::newRow("foo has priority") << QStringList{"./foo", "./bar"} << "foo";
    QTest::newRow("bar has priority") << QStringList{"./bar", "./foo"} << "bar";
}

void TestLanguage::modulePrioritizationBySearchPath()
{
    QFETCH(QStringList, searchPaths);
    QFETCH(QString, expectedVariant);

    bool exceptionCaught = false;
    try {
        defaultParameters.setOverriddenValues(
            {std::make_pair(QStringLiteral("project.qbsSearchPaths"),
                            searchPaths)});
        resolveProject("module-prioritization-by-search-path/project.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 1);
        ResolvedProductPtr product = products.values().front();
        QVERIFY(!!product);
        const QString actualVariant = product->moduleProperties->moduleProperty
                ("conflicting-instances", "moduleVariant").toString();
        QCOMPARE(actualVariant, expectedVariant);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QVERIFY(!exceptionCaught);
}

void TestLanguage::moduleProperties_data()
{
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<QVariant>("expectedValue");
    QTest::newRow("init") << QString() << QVariant();
    QTest::newRow("merge_lists")
            << "defines"
            << QVariant(QStringList() << "THE_PRODUCT" << "QT_NETWORK" << "QT_GUI" << "QT_CORE");
    QTest::newRow("merge_lists_and_values")
            << "defines"
            << QVariant(QStringList() << "THE_PRODUCT" << "QT_NETWORK" << "QT_GUI" << "QT_CORE");
    QTest::newRow("merge_lists_with_duplicates")
            << "cxxFlags"
            << QVariant(QStringList() << "-foo" << "BAR" << "-foo" << "BAZ");
    QTest::newRow("merge_lists_with_prototype_values")
            << "rpaths"
            << QVariant(QStringList() << "/opt/qt/lib" << "$ORIGIN");
    QTest::newRow("list_property_that_references_product")
            << "listProp"
            << QVariant(QStringList() << "x" << "123");
    QTest::newRow("list_property_depending_on_overridden_property")
            << "listProp2"
            << QVariant(QStringList() << "PRODUCT_STUFF" << "DEFAULT_STUFF" << "EXTRA_STUFF");
    QTest::newRow("overridden_list_property")
            << "listProp"
            << QVariant(QStringList() << "PRODUCT_STUFF");
    QTest::newRow("shadowed-list-property")
            << "defines"
            << QVariant(QStringList() << "MyProject" << "shadowed-list-property");
    QTest::newRow("shadowed-scalar-property")
            << "someString"
            << QVariant(QString("MyProject_shadowed-scalar-property"));
    QTest::newRow("merged-varlist")
            << "varListProp"
            << QVariant(QVariantList() << QVariantMap({std::make_pair("d", "product")})
                        << QVariantMap({std::make_pair("c", "qtcore")})
                        << QVariantMap({std::make_pair("a", true),
                                        std::make_pair("b", QVariant())}));
    QTest::newRow("cleanup") << QString() << QVariant();
}

void TestLanguage::moduleProperties()
{
    HANDLE_INIT_CLEANUP_DATATAGS("moduleproperties.qbs");
    QFETCH(QString, propertyName);
    QFETCH(QVariant, expectedValue);
    QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
    const QString productName = QString::fromLocal8Bit(QTest::currentDataTag());
    ResolvedProductPtr product = products.value(productName);
    QVERIFY(!!product);
    const QVariant value = product->moduleProperties->moduleProperty("dummy", propertyName);
    QCOMPARE(value, expectedValue);
}

void TestLanguage::modulePropertiesInGroups()
{
    defaultParameters.setProjectFilePath(testProject("modulepropertiesingroups.qbs"));
    bool exceptionCaught = false;
    try {
        resolveProject();
        QVERIFY(!!project);
        const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        ResolvedProductPtr product = products.value("grouptest");
        QVERIFY(!!product);
        GroupConstPtr g1;
        GroupConstPtr g11;
        GroupConstPtr g12;
        GroupConstPtr g2;
        GroupConstPtr g21;
        GroupConstPtr g211;
        for (const GroupPtr &g : product->groups) {
            if (g->name == "g1")
                g1= g;
            else if (g->name == "g2")
                g2 = g;
            else if (g->name == "g1.1")
                g11 = g;
            else if (g->name == "g1.2")
                g12 = g;
            else if (g->name == "g2.1")
                g21 = g;
            else if (g->name == "g2.1.1")
                g211 = g;
        }
        QVERIFY(!!g1);
        QVERIFY(!!g2);
        QVERIFY(!!g11);
        QVERIFY(!!g12);
        QVERIFY(!!g21);
        QVERIFY(!!g211);

        const QVariantMap productProps = product->moduleProperties->value();
        const auto &productGmod1List1 = moduleProperty(productProps, "gmod.gmod1", "gmod1_list1")
                .toStringList();
        QCOMPARE(productGmod1List1, QStringList() << "gmod1_list1_proto" << "gmod1_string_proto");
        const auto &productGmod1List2 = moduleProperty(productProps, "gmod.gmod1", "gmod1_list2")
                .toStringList();
        QCOMPARE(productGmod1List2, QStringList() << "grouptest" << "gmod1_string_proto"
                 << "gmod1_list2_proto");
        const auto &productGmod1List3 = moduleProperty(productProps, "gmod.gmod1", "gmod1_list3")
                .toStringList();
        QCOMPARE(productGmod1List3, QStringList() << "product" << "gmod1_string_proto");
        const int productP0 = moduleProperty(productProps, "gmod.gmod1", "p0").toInt();
        QCOMPARE(productP0, 1);
        const int productDepProp = moduleProperty(productProps, "gmod.gmod1", "depProp").toInt();
        QCOMPARE(productDepProp, 0);
        const auto &productGmod2String = moduleProperty(productProps, "gmod2", "gmod2_string")
                .toString();
        QCOMPARE(productGmod2String, QString("gmod1_string_proto"));
        const auto &productGmod2List = moduleProperty(productProps, "gmod2", "gmod2_list")
                .toStringList();
        QCOMPARE(productGmod2List, QStringList() << "gmod1_string_proto" << "commonName_in_gmod1"
                 << "gmod4_string_proto_gmod3_string_proto"  << "gmod3_string_proto"
                 << "gmod2_list_proto");

        const QVariantMap g1Props = g1->properties->value();
        const auto &g1Gmod1List1 = moduleProperty(g1Props, "gmod.gmod1", "gmod1_list1")
                .toStringList();
        QCOMPARE(g1Gmod1List1, QStringList() << "gmod1_list1_proto" << "g1");
        const auto &g1Gmod1List2 = moduleProperty(g1Props, "gmod.gmod1", "gmod1_list2")
                .toStringList();
        QCOMPARE(g1Gmod1List2, QStringList() << "grouptest" << "gmod1_string_proto"
                 << "gmod1_list2_proto" << "g1");
        const auto &g1Gmod1List3 = moduleProperty(g1Props, "gmod.gmod1", "gmod1_list3")
                .toStringList();
        QCOMPARE(g1Gmod1List3, QStringList() << "product" << "g1");
        const int g1P0 = moduleProperty(g1Props, "gmod.gmod1", "p0").toInt();
        QCOMPARE(g1P0, 3);
        const int g1DepProp = moduleProperty(g1Props, "gmod.gmod1", "depProp").toInt();
        QCOMPARE(g1DepProp, 1);
        const auto &g1Gmod2String = moduleProperty(g1Props, "gmod2", "gmod2_string").toString();
        QCOMPARE(g1Gmod2String, QString("g1"));
        const auto &g1Gmod2List = moduleProperty(g1Props, "gmod2", "gmod2_list")
                .toStringList();
        QCOMPARE(g1Gmod2List, QStringList() << "g1" << "commonName_in_gmod1" << "g1_gmod4_g1_gmod3"
                 << "g1_gmod3" << "gmod2_list_proto");

        const QVariantMap g11Props = g11->properties->value();
        const auto &g11Gmod1List1 = moduleProperty(g11Props, "gmod.gmod1", "gmod1_list1")
                .toStringList();
        QCOMPARE(g11Gmod1List1, QStringList() << "gmod1_list1_proto" << "g1.1");
        const auto &g11Gmod1List2 = moduleProperty(g11Props, "gmod.gmod1", "gmod1_list2")
                .toStringList();
        QCOMPARE(g11Gmod1List2, QStringList() << "grouptest" << "gmod1_string_proto"
                 << "gmod1_list2_proto" << "g1" << "g1.1");
        const auto &g11Gmod1List3 = moduleProperty(g11Props, "gmod.gmod1", "gmod1_list3")
                .toStringList();
        QCOMPARE(g11Gmod1List3, QStringList() << "product" << "g1.1");
        const int g11P0 = moduleProperty(g11Props, "gmod.gmod1", "p0").toInt();
        QCOMPARE(g11P0, 5);
        const int g11DepProp = moduleProperty(g11Props, "gmod.gmod1", "depProp").toInt();
        QCOMPARE(g11DepProp, 2);
        const auto &g11Gmod2String = moduleProperty(g11Props, "gmod2", "gmod2_string").toString();
        QCOMPARE(g11Gmod2String, QString("g1.1"));
        const auto &g11Gmod2List = moduleProperty(g11Props, "gmod2", "gmod2_list")
                .toStringList();
        QCOMPARE(g11Gmod2List, QStringList() << "g1.1" << "commonName_in_gmod1"
                 << "g1.1_gmod4_g1.1_gmod3" << "g1.1_gmod3" << "gmod2_list_proto");

        const QVariantMap g12Props = g12->properties->value();
        const auto &g12Gmod1List1 = moduleProperty(g12Props, "gmod.gmod1", "gmod1_list1")
                .toStringList();
        QCOMPARE(g12Gmod1List1, QStringList() << "gmod1_list1_proto" << "g1.2");
        const auto &g12Gmod1List2 = moduleProperty(g12Props, "gmod.gmod1", "gmod1_list2")
                .toStringList();
        QCOMPARE(g12Gmod1List2, QStringList() << "grouptest" << "gmod1_string_proto"
                 << "gmod1_list2_proto" << "g1" << "g1.2");
        const auto &g12Gmod1List3 = moduleProperty(g12Props, "gmod.gmod1", "gmod1_list3")
                .toStringList();
        QCOMPARE(g12Gmod1List3, QStringList() << "product" << "g1.2");
        const int g12P0 = moduleProperty(g12Props, "gmod.gmod1", "p0").toInt();
        QCOMPARE(g12P0, 9);
        const int g12DepProp = moduleProperty(g12Props, "gmod.gmod1", "depProp").toInt();
        QCOMPARE(g12DepProp, 1);
        const auto &g12Gmod2String = moduleProperty(g12Props, "gmod2", "gmod2_string").toString();
        QCOMPARE(g12Gmod2String, QString("g1.2"));
        const auto &g12Gmod2List = moduleProperty(g12Props, "gmod2", "gmod2_list")
                .toStringList();
        QCOMPARE(g12Gmod2List, QStringList() << "g1.2" << "commonName_in_gmod1"
                 << "g1_gmod4_g1.2_gmod3" << "g1.2_gmod3" << "gmod2_list_proto");

        const QVariantMap g2Props = g2->properties->value();
        const auto &g2Gmod1List1 = moduleProperty(g2Props, "gmod.gmod1", "gmod1_list1")
                .toStringList();
        QCOMPARE(g2Gmod1List1, QStringList() << "gmod1_list1_proto" << "g2");
        const auto &g2Gmod1List2 = moduleProperty(g2Props, "gmod.gmod1", "gmod1_list2")
                .toStringList();
        QCOMPARE(g2Gmod1List2, QStringList() << "grouptest" << "gmod1_string_proto" << "gmod1_list2_proto");
        const int g2P0 = moduleProperty(g2Props, "gmod.gmod1", "p0").toInt();
        QCOMPARE(g2P0, 6);
        const int g2DepProp = moduleProperty(g2Props, "gmod.gmod1", "depProp").toInt();
        QCOMPARE(g2DepProp, 2);
        const auto &g2Gmod2String = moduleProperty(g2Props, "gmod2", "gmod2_string").toString();
        QCOMPARE(g2Gmod2String, QString("g2"));
        const auto &g2Gmod2List = moduleProperty(g2Props, "gmod2", "gmod2_list")
                .toStringList();
        QCOMPARE(g2Gmod2List, QStringList() << "g2" << "commonName_in_gmod1" << "g2_gmod4_g2_gmod3"
                 << "g2_gmod3" << "gmod2_list_proto");

        const QVariantMap g21Props = g21->properties->value();
        const auto &g21Gmod1List1 = moduleProperty(g21Props, "gmod.gmod1", "gmod1_list1")
                .toStringList();
        QCOMPARE(g21Gmod1List1, QStringList() << "gmod1_list1_proto" << "g2");
        const auto &g21Gmod1List2 = moduleProperty(g21Props, "gmod.gmod1", "gmod1_list2")
                .toStringList();
        QEXPECT_FAIL(nullptr, "no re-eval when no module props set", Continue);
        QCOMPARE(g21Gmod1List2, QStringList() << "grouptest" << "g2.1" << "gmod1_list2_proto");
        const int g21P0 = moduleProperty(g21Props, "gmod.gmod1", "p0").toInt();
        QCOMPARE(g21P0, 6);
        const int g21DepProp = moduleProperty(g21Props, "gmod.gmod1", "depProp").toInt();
        QCOMPARE(g21DepProp, 2);
        const auto &g21Gmod2String = moduleProperty(g21Props, "gmod2", "gmod2_string").toString();
        QCOMPARE(g21Gmod2String, QString("g2"));
        const auto &g21Gmod2List = moduleProperty(g21Props, "gmod2", "gmod2_list")
                .toStringList();
        QEXPECT_FAIL(nullptr, "no re-eval when no module props set", Continue);
        QCOMPARE(g21Gmod2List, QStringList() << "g2" << "commonName_in_gmod1"
                 << "g2.1_gmod4_g2.1_gmod3" << "g2.1_gmod3" << "gmod2_list_proto");

        const QVariantMap g211Props = g211->properties->value();
        const auto &g211Gmod1List1 = moduleProperty(g211Props, "gmod.gmod1", "gmod1_list1")
                .toStringList();
        QCOMPARE(g211Gmod1List1, QStringList() << "gmod1_list1_proto" << "g2");
        const auto &g211Gmod1List2 = moduleProperty(g211Props, "gmod.gmod1", "gmod1_list2")
                .toStringList();
        QCOMPARE(g211Gmod1List2, QStringList() << "g2.1.1");
        const int g211P0 = moduleProperty(g211Props, "gmod.gmod1", "p0").toInt();
        QCOMPARE(g211P0, 17);
        const int g211DepProp = moduleProperty(g211Props, "gmod.gmod1", "depProp").toInt();
        QCOMPARE(g211DepProp, 2);
        const auto &g211Gmod2String
                = moduleProperty(g211Props, "gmod2", "gmod2_string").toString();
        QEXPECT_FAIL(nullptr, "re-eval not triggered", Continue);
        QCOMPARE(g211Gmod2String, QString("g2.1.1"));
        const auto &g211Gmod2List = moduleProperty(g211Props, "gmod2", "gmod2_list")
                .toStringList();
        QEXPECT_FAIL(nullptr, "re-eval not triggered", Continue);
        QCOMPARE(g211Gmod2List, QStringList() << "g2.1.1" << "commonName_in_gmod1"
                 << "g2.1.1_gmod4_g2.1.1_gmod3" << "g2.1.1_gmod3" << "gmod2_list_proto");

        product = products.value("grouptest2");
        QVERIFY(!!product);
        g1.reset();
        g11.reset();
        for (const GroupPtr &g : product->groups) {
            if (g->name == "g1")
                g1= g;
            else if (g->name == "g1.1")
                g11 = g;
        }
        QVERIFY(!!g1);
        QVERIFY(!!g11);
        QCOMPARE(moduleProperty(g1->properties->value(), "gmod.gmod1", "gmod1_list2")
                 .toStringList(), QStringList({"G1"}));
        QCOMPARE(moduleProperty(g11->properties->value(), "gmod.gmod1", "gmod1_list2")
                 .toStringList(),
                 moduleProperty(g1->properties->value(), "gmod.gmod1", "gmod1_list2")
                 .toStringList());
        QCOMPARE(moduleProperty(g11->properties->value(), "gmod.gmod1", "gmod1_string").toString(),
                 QString("G1.1"));
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::modulePropertyOverridesPerProduct()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setOverriddenValues({
                std::make_pair("modules.dummy.rpaths", QStringList({"/usr/lib"})),
                std::make_pair("modules.dummy.someString", "m"),
                std::make_pair("products.b.dummy.someString", "b"),
                std::make_pair("products.c.dummy.someString", "c"),
                std::make_pair("products.c.dummy.rpaths", QStringList({"/home", "/tmp"}))
        });
        resolveProject("module-property-overrides-per-product.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 3);
        const ResolvedProductConstPtr a = products.value("a");
        QVERIFY(!!a);
        const ResolvedProductConstPtr b = products.value("b");
        QVERIFY(!!b);
        const ResolvedProductConstPtr c = products.value("c");
        QVERIFY(!!c);

        const auto stringPropertyValue = [](const ResolvedProductConstPtr &p) -> QString
        {
            return p->moduleProperties->moduleProperty("dummy", "someString").toString();
        };
        const auto listPropertyValue = [](const ResolvedProductConstPtr &p) -> QStringList
        {
            return p->moduleProperties->moduleProperty("dummy", "rpaths").toStringList();
        };
        const auto productPropertyValue = [](const ResolvedProductConstPtr &p) -> QStringList
        {
            return p->productProperties.value("rpaths").toStringList();
        };

        QCOMPARE(stringPropertyValue(a), QString("m"));
        QCOMPARE(stringPropertyValue(b), QString("b"));
        QCOMPARE(stringPropertyValue(c), QString("c"));
        QCOMPARE(listPropertyValue(a), QStringList({"/usr/lib"}));
        QCOMPARE(listPropertyValue(b), QStringList({"/usr/lib"}));
        QCOMPARE(listPropertyValue(c), QStringList({"/home", "/tmp"}));
        QCOMPARE(listPropertyValue(a), productPropertyValue(a));
        QCOMPARE(listPropertyValue(b), productPropertyValue(b));
        QCOMPARE(listPropertyValue(c), productPropertyValue(c));
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::moduleScope()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("modulescope.qbs"));
        resolveProject();
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 1);
        ResolvedProductPtr product = products.value("product1");
        QVERIFY(!!product);

        auto intModuleValue = [product] (const QString &name) -> int
        {
            return product->moduleProperties->moduleProperty("scopemod", name).toInt();
        };

        QCOMPARE(intModuleValue("a"), 2);     // overridden in module instance
        QCOMPARE(intModuleValue("b"), 1);     // genuine
        QCOMPARE(intModuleValue("c"), 3);     // genuine, dependent on overridden value
        QCOMPARE(intModuleValue("d"), 2);     // genuine, dependent on genuine value
        QCOMPARE(intModuleValue("e"), 1);     // genuine
        QCOMPARE(intModuleValue("f"), 2);     // overridden
        QCOMPARE(intModuleValue("g"), 156);   // overridden, dependent on product properties
        QCOMPARE(intModuleValue("h"), 158);   // overridden, base dependent on product properties
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::moduleWithProductDependency()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("module-depends-on-product.qbs"));
        resolveProject();
        QVERIFY(project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 2);
        ResolvedProductPtr product = products.value("p1");
        QVERIFY(product);
        QCOMPARE(int(product->dependencies.size()), 1);
    } catch (const ErrorInfo &e) {
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
            << (QStringList() << "qbs" << "deepdummy.deep.moat" << "dummy")
            << QString("abysmal");
    QTest::newRow("deep_module_name_submodule_syntax1")
            << (QStringList() << "qbs" << "deepdummy.deep.moat" << "dummy")
            << QString("abysmal");
    QTest::newRow("deep_module_name_submodule_syntax2")
            << (QStringList() << "qbs" << "deepdummy.deep.moat" << "dummy")
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
    const ResolvedProductPtr product = products.value(productName);
    QVERIFY(!!product);
    QCOMPARE(product->name, productName);
    QStringList modulesInProduct;
    for (ResolvedModuleConstPtr m : product->modules)
        modulesInProduct += m->name;
    modulesInProduct.sort();
    expectedModulesInProduct.sort();
    QCOMPARE(modulesInProduct, expectedModulesInProduct);
    QCOMPARE(product->productProperties.value("foo").toString(), expectedProductProperty);
}

void TestLanguage::multiplexedExports()
{
    bool exceptionCaught = false;
    try {
        resolveProject("multiplexed-exports.qbs");
        QVERIFY(!!project);
        const auto products = project->allProducts();
        QCOMPARE(products.size(), size_t(4));
        std::set<ResolvedProductPtr> pVariants;
        for (const auto &product : products) {
            if (product->name != "p")
                continue;
            static const auto buildVariant = [](const ResolvedProductConstPtr &p) {
                return p->moduleProperties->qbsPropertyValue("buildVariant").toString();
            };
            static const auto cppIncludePaths = [](const ResolvedProductConstPtr &p) {
                return p->moduleProperties->moduleProperty("cpp", "includePaths").toStringList();
            };
            if (buildVariant(product) == "debug") {
                pVariants.insert(product);
                QCOMPARE(cppIncludePaths(product), QStringList("/d"));
            } else if (buildVariant(product) == "release") {
                pVariants.insert(product);
                QCOMPARE(cppIncludePaths(product), QStringList("/r"));
            }
        }
        QCOMPARE(int(pVariants.size()), 2);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::multiplexingByProfile()
{
    QFETCH(QString, projectFileName);
    QFETCH(bool, successExpected);
    try {
        defaultParameters.setDryRun(true);
        resolveProject(qPrintable("/multiplexing-by-profile/" + projectFileName));
        QVERIFY(successExpected);
        QVERIFY(!!project);
    } catch (const ErrorInfo &e) {
        QVERIFY2(!successExpected, qPrintable(e.toString()));
    }
}

void TestLanguage::multiplexingByProfile_data()
{
    QTest::addColumn<QString>("projectFileName");
    QTest::addColumn<bool>("successExpected");
    QTest::newRow("same profile") << "p1.qbs" << true;
    QTest::newRow("dependency on non-multiplexed") << "p2.qbs" << true;
    QTest::newRow("dependency by non-multiplexed") << "p3.qbs" << false;
    QTest::newRow("dependency by non-multiplexed with Depends.profile") << "p4.qbs" << true;
}

void TestLanguage::nonApplicableModulePropertyInProfile()
{
    QFETCH(QString, targetOS);
    QFETCH(QString, toolchain);
    QFETCH(bool, successExpected);
    try {
        defaultParameters.setOverriddenValues({std::make_pair("project.targetOS", targetOS),
                                               std::make_pair("project.toolchain", toolchain)});
        resolveProject("non-applicable-module-property-in-profile.qbs");
        QVERIFY(!!project);
        QVERIFY(successExpected);
    } catch (const ErrorInfo &e) {
        QVERIFY2(!successExpected, qPrintable(e.toString()));
        QVERIFY2(e.toString().contains("Loading module 'multiple_backends' for product 'p' failed "
                                       "due to invalid values in profile 'theProfile'"),
                 qPrintable(e.toString()));
        QVERIFY2(e.toString().contains("backend3Prop"), qPrintable(e.toString()));
    }
}

void TestLanguage::nonApplicableModulePropertyInProfile_data()
{
    QTest::addColumn<QString>("targetOS");
    QTest::addColumn<QString>("toolchain");
    QTest::addColumn<bool>("successExpected");

    QTest::newRow("no matching property (1)") << "os1" << QString() << false;
    QTest::newRow("no matching property (2)") << "os2" << QString() << false;

    // The point here is that there's a second, lower-prioritized candidate with a matching
    // condition that doesn't have the property. This candidate must not throw an error.
    QTest::newRow("matching property") << "os2" << "tc" << true;
}

void TestLanguage::nonRequiredProducts()
{
    bool exceptionCaught = false;
    try {
        QFETCH(bool, subProjectEnabled);
        QFETCH(bool, dependeeEnabled);
        QVariantMap overriddenValues;
        if (!subProjectEnabled)
            overriddenValues.insert("projects.subproject.condition", false);
        else if (!dependeeEnabled)
            overriddenValues.insert("products.dependee.condition", false);
        defaultParameters.setOverriddenValues(overriddenValues);
        resolveProject("non-required-products.qbs");
        QVERIFY(!!project);
        const auto products = productsFromProject(project);
        QCOMPARE(products.size(), 4 + !!subProjectEnabled);
        const ResolvedProductConstPtr dependee = products.value("dependee");
        QCOMPARE(subProjectEnabled, !!dependee);
        if (dependee)
            QCOMPARE(dependeeEnabled, dependee->enabled);
        const ResolvedProductConstPtr depender = products.value("depender");
        QVERIFY(!!depender);
        const QStringList defines = depender->moduleProperties->moduleProperty("dummy", "defines")
                .toStringList();
        QCOMPARE(subProjectEnabled && dependeeEnabled, defines.contains("WITH_DEPENDEE"));

        for (const auto &name : std::vector<const char *>({ "p3", "p2", "p1"})) {
             const ResolvedProductConstPtr &product = products.value(name);
             QVERIFY2(product, name);
             QVERIFY2(!product->enabled, name);
        }
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::nonRequiredProducts_data()
{
    QTest::addColumn<bool>("subProjectEnabled");
    QTest::addColumn<bool>("dependeeEnabled");
    QTest::newRow("dependee enabled") << true << true;
    QTest::newRow("dependee disabled") << true << false;
    QTest::newRow("sub project disabled") << false << true;
}

void TestLanguage::outerInGroup()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("outerInGroup.qbs"));
        resolveProject();
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 1);
        ResolvedProductPtr product = products.value("OuterInGroup");
        QVERIFY(!!product);
        QCOMPARE(product->groups.size(), size_t(2));
        GroupPtr group = product->groups.at(0);
        QVERIFY(!!group);
        QCOMPARE(group->name, product->name);
        QVERIFY(group->files);
        QCOMPARE(group->files->size(), size_t(1));
        SourceArtifactConstPtr artifact = group->files->front();
        QVariant installDir = artifact->properties->qbsPropertyValue("installDir");
        QCOMPARE(installDir.toString(), QString("/somewhere"));
        group = product->groups.at(1);
        QVERIFY(!!group);
        QCOMPARE(group->name, QString("Special Group"));
        QVERIFY(group->files);
        QCOMPARE(group->files->size(), size_t(1));
        artifact = group->files->front();
        installDir = artifact->properties->qbsPropertyValue("installDir");
        QCOMPARE(installDir.toString(), QString("/somewhere/else"));
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::overriddenPropertiesAndPrototypes()
{
    bool exceptionCaught = false;
    try {
        QFETCH(QString, osName);
        QFETCH(QString, backendName);
        defaultParameters.setOverriddenValues({std::make_pair("modules.qbs.targetPlatform",
                                                              osName)});
        resolveProject("overridden-properties-and-prototypes.qbs");
        QVERIFY(!!project);
        QCOMPARE(project->products.size(), size_t(1));
        QCOMPARE(project->products.front()->moduleProperties->moduleProperty(
                     "multiple_backends", "prop").toString(), backendName);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::overriddenPropertiesAndPrototypes_data()
{
    QTest::addColumn<QString>("osName");
    QTest::addColumn<QString>("backendName");
    QTest::newRow("first backend") << "os1" << "backend 1";
    QTest::newRow("second backend") << "os2" << "backend 2";
}

void TestLanguage::overriddenVariantProperty()
{
    bool exceptionCaught = false;
    try {
        const QVariantMap objectValue{std::make_pair("x", 1), std::make_pair("y", 2)};
        defaultParameters.setOverriddenValues({std::make_pair("products.p.myObject", objectValue)});
        resolveProject("overridden-variant-property.qbs");
        QVERIFY(!!project);
        QCOMPARE(project->products.size(), size_t(1));
        QCOMPARE(project->products.front()->productProperties.value("myObject").toMap(),
                 objectValue);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::parameterTypes()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setProjectFilePath(testProject("parameter-types.qbs"));
        resolveProject();
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::pathProperties()
{
    bool exceptionCaught = false;
    try {
        resolveProject("pathproperties.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        ResolvedProductPtr product = products.value("product1");
        QVERIFY(!!product);
        QString projectFileDir = QFileInfo(defaultParameters.projectFilePath()).absolutePath();
        const QVariantMap productProps = product->productProperties;
        QCOMPARE(productProps.value("projectFileDir").toString(), projectFileDir);
        QStringList filesInProjectFileDir = QStringList()
                << FileInfo::resolvePath(projectFileDir, "aboutdialog.h")
                << FileInfo::resolvePath(projectFileDir, "aboutdialog.cpp");
        QCOMPARE(productProps.value("filesInProjectFileDir").toStringList(), filesInProjectFileDir);
        QStringList includePaths = product->moduleProperties->property(
                QStringList() << "dummy" << "includePaths").toStringList();
        QCOMPARE(includePaths, QStringList() << projectFileDir);
        QCOMPARE(productProps.value("base_fileInProductDir").toString(),
                 FileInfo::resolvePath(projectFileDir, QStringLiteral("foo")));
        QCOMPARE(productProps.value("base_fileInBaseProductDir").toString(),
                 FileInfo::resolvePath(projectFileDir, QStringLiteral("subdir/bar")));
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::probesAndMultiplexing()
{
    bool exceptionCaught = false;
    try {
        resolveProject("probes-and-multiplexing.qbs");
        QVERIFY(project);
        QCOMPARE(int(project->products.size()), 3);
        QStringList architectures{"x86", "x86_64", "arm"};
        for (const ResolvedProductPtr &product : project->products) {
             const QString arch = product->moduleProperties->moduleProperty("qbs", "architecture")
                                      .toString();
             QVERIFY2(architectures.removeOne(arch), qPrintable(arch));
             QCOMPARE(product->productProperties.value("archFromProbe").toString(), arch);
             bool foundGroup = false;
             for (const GroupPtr &group : product->groups) {
                if (group->name == "theGroup") {
                    foundGroup = true;
                    QCOMPARE(group->properties->moduleProperty("qbs", "sysroot"), "/" + arch);
                }
             }
             QVERIFY(foundGroup);
        }
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
        TemporaryProfile tp(QStringLiteral("tst_lang_profile"), m_settings);
        Profile profile = tp.p;
        profile.setValue("dummy.defines", "IN_PROFILE");
        profile.setValue("dummy.cFlags", "IN_PROFILE");
        profile.setValue("dummy.cxxFlags", "IN_PROFILE");
        profile.setValue("qbs.architecture", "x86");
        defaultParameters.setTopLevelProfile(profile.name());
        QVariantMap overriddenValues;
        overriddenValues.insert("modules.dummy.cFlags", "OVERRIDDEN");
        defaultParameters.setOverriddenValues(overriddenValues);
        resolveProject("profilevaluesandoverriddenvalues.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        ResolvedProductPtr product = products.value("product1");
        QVERIFY(!!product);
        QVariantList values;
        values = product->moduleProperties->moduleProperty("dummy", "cxxFlags").toList();
        QCOMPARE(values.length(), 1);
        QCOMPARE(values.front().toString(), QString("IN_PROFILE"));
        values = product->moduleProperties->moduleProperty("dummy", "defines").toList();
        QCOMPARE(values, QVariantList() << QStringLiteral("IN_FILE") << QStringLiteral("IN_PROFILE"));
        values = product->moduleProperties->moduleProperty("dummy", "cFlags").toList();
        QCOMPARE(values.length(), 1);
        QCOMPARE(values.front().toString(), QString("OVERRIDDEN"));
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::projectFileLookup()
{
    QFETCH(QString, projectFileInput);
    QFETCH(QString, projectFileOutput);
    QFETCH(bool, failureExpected);

    try {
        SetupProjectParameters params;
        params.setProjectFilePath(projectFileInput);
        params.finalizeProjectFilePath();
        QVERIFY(!failureExpected);
        QCOMPARE(params.projectFilePath(), projectFileOutput);
    } catch (const ErrorInfo &) {
        QVERIFY(failureExpected);
    }
}

void TestLanguage::projectFileLookup_data()
{
    QTest::addColumn<QString>("projectFileInput");
    QTest::addColumn<QString>("projectFileOutput");
    QTest::addColumn<bool>("failureExpected");

    const QString baseDir = testDataDir();
    const QString multiProjectsDir = baseDir + "/dirwithmultipleprojects";
    const QString noProjectsDir = baseDir + "/dirwithnoprojects";
    const QString oneProjectDir = baseDir + "/dirwithoneproject";
    QVERIFY(QDir(noProjectsDir).exists() && QDir(oneProjectDir).exists()
            && QDir(multiProjectsDir).exists());
    const QString fullFilePath = multiProjectsDir + "/project.qbs";
    QTest::newRow("full file path") << fullFilePath << fullFilePath << false;
    QTest::newRow("base dir ") << oneProjectDir << (oneProjectDir + "/project.qbs") << false;
    QTest::newRow("empty dir") << noProjectsDir << QString() << true;
    QTest::newRow("ambiguous dir") << multiProjectsDir << QString() << true;
}

void TestLanguage::productConditions()
{
    bool exceptionCaught = false;
    try {
        resolveProject("productconditions.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 6);
        ResolvedProductPtr product;
        product = products.value("product_no_condition");
        QVERIFY(!!product);
        QVERIFY(product->enabled);

        product = products.value("product_true_condition");
        QVERIFY(!!product);
        QVERIFY(product->enabled);

        product = products.value("product_condition_dependent_of_module");
        QVERIFY(!!product);
        QVERIFY(product->enabled);

        product = products.value("product_false_condition");
        QVERIFY(!!product);
        QVERIFY(!product->enabled);

        product = products.value("product_probe_condition_false");
        QVERIFY(!!product);
        QVERIFY(!product->enabled);

        product = products.value("product_probe_condition_true");
        QVERIFY(!!product);
        QVERIFY(product->enabled);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::productDirectories()
{
    bool exceptionCaught = false;
    try {
        resolveProject("productdirectories.qbs");
        QVERIFY(!!project);
        QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 1);
        ResolvedProductPtr product;
        product = products.value("MyApp");
        QVERIFY(!!product);
        const QVariantMap config = product->productProperties;
        QCOMPARE(config.value(QStringLiteral("buildDirectory")).toString(),
                 product->buildDirectory());
        QCOMPARE(config.value(QStringLiteral("sourceDirectory")).toString(), testDataDir());
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::propertiesBlocks_data()
{
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<QVariant>("expectedValue");
    QTest::addColumn<QString>("expectedStringValue");

    QTest::newRow("init") << QString() << QVariant() << QString();
    QTest::newRow("property_append")
        << QString("dummy.defines") << QVariant(QStringList{"APPENDED", "SOMETHING"}) << QString();
    QTest::newRow("property_set_indirect")
            << QString("dummy.cFlags")
            << QVariant(QStringList("VAL"))
            << QString();
    QTest::newRow("property_overwrite")
        << QString("dummy.defines") << QVariant(QStringList("OVERWRITTEN")) << QString();
    QTest::newRow("property_append_indirect")
        << QString("dummy.defines") << QVariant(QStringList{"TWO", "ONE"}) << QString();
    QTest::newRow("property_append_to_indirect_derived")
        << QString("dummy.cFlags") << QVariant(QStringList{"BASE", "PROPS"}) << QString();
    QTest::newRow("property_append_to_indirect_derived2")
        << QString("dummy.cFlags") << QVariant(QStringList{"PROPS", "PRODUCT"}) << QString();
    QTest::newRow("property_append_to_indirect_derived3")
        << QString("dummy.cFlags") << QVariant(QStringList{"PROPS", "BASE", "PRODUCT"})
        << QString();
    QTest::newRow("property_append_to_indirect_merged")
        << QString("dummy.rpaths") << QVariant(QStringList{"TWO", "ONE", "$ORIGIN"}) << QString();

    QTest::newRow("multiple_exclusive_properties")
            << QString("dummy.defines")
            << QVariant(QStringList("OVERWRITTEN"))
            << QString();
    QTest::newRow("multiple_exclusive_properties_no_match")
        << QString("dummy.defines") << QVariant(QStringList("OVERWRITTEN")) << QString();
    QTest::newRow("multiple_exclusive_properties_append")
        << QString("dummy.defines") << QVariant(QStringList{"TWO", "ONE"}) << QString();

    QTest::newRow("condition_refers_to_product_property")
            << QString("dummy.defines")
            << QVariant(QStringList("OVERWRITTEN"))
            << QString("OVERWRITTEN");
    QTest::newRow("condition_refers_to_project_property")
            << QString("dummy.defines")
            << QVariant(QStringList("OVERWRITTEN"))
            << QString("OVERWRITTEN");

    QTest::newRow("ambiguous_properties")
        << QString("dummy.defines") << QVariant(QStringList{"TWO", "THREE", "ONE"}) << QString();
    QTest::newRow("inheritance_overwrite_in_subitem")
            << QString("dummy.defines")
            << QVariant(QStringList() << QString("OVERWRITTEN_IN_SUBITEM"))
            << QString();
    QTest::newRow("inheritance_retain_base1")
        << QString("dummy.defines") << QVariant(QStringList{"BASE", "SOMETHING", "SUB"})
        << QString();
    QTest::newRow("inheritance_retain_base2")
        << QString("dummy.defines") << QVariant(QStringList{"BASE", "SOMETHING", "SUB"})
        << QString();
    QTest::newRow("inheritance_retain_base3")
        << QString("dummy.defines") << QVariant(QStringList{"BASE", "SOMETHING", "SUB"})
        << QString();
    QTest::newRow("inheritance_retain_base4")
        << QString("dummy.defines") << QVariant(QStringList{"BASE", "SOMETHING"}) << QString();
    QTest::newRow("inheritance_condition_in_subitem1")
            << QString("dummy.defines")
            << QVariant(QStringList() << QString("SOMETHING") << QString("SUB"))
            << QString();
    QTest::newRow("inheritance_condition_in_subitem2")
            << QString("dummy.defines")
            << QVariant(QStringList() << QString("SOMETHING"))
            << QString();
    QTest::newRow("condition_references_id")
            << QString("dummy.defines")
            << QVariant(QStringList() << QString("OVERWRITTEN"))
            << QString();
    QTest::newRow("using_derived_Properties_item") << "dummy.defines"
            << QVariant(QStringList() << "string from MyProperties") << QString();
    QTest::newRow("conditional-depends")
            << QString("dummy.defines")
            << QVariant()
            << QString();
    QTest::newRow("use-module-with-properties-item")
            << QString("module-with-properties-item.stringProperty")
            << QVariant(QString("overridden in Properties item"))
            << QString();
    QTest::newRow("cleanup") << QString() << QVariant() << QString();
}

void TestLanguage::propertiesBlocks()
{
    defaultParameters.setDeprecationWarningMode(DeprecationWarningMode::On);
    HANDLE_INIT_CLEANUP_DATATAGS("propertiesblocks.qbs");
    QFETCH(QString, propertyName);
    QFETCH(QVariant, expectedValue);
    QFETCH(QString, expectedStringValue);
    QVERIFY(!!project);

    QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
    const QString productName = QString::fromLocal8Bit(QTest::currentDataTag());
    ResolvedProductPtr product = products.value(productName);
    QVERIFY(!!product);
    QCOMPARE(product->name, productName);
    QVariant v = productPropertyValue(product, propertyName);
    QCOMPARE(v, expectedValue);
    if (!expectedStringValue.isEmpty()) {
        v = productPropertyValue(product, "someString");
        QCOMPARE(v.toString(), expectedStringValue);
    }
}

void TestLanguage::propertiesBlockInGroup_data()
{
    QTest::addColumn<bool>("withGroup");
    QTest::addColumn<QString>("moduleGroup");
    QTest::addColumn<QStringList>("expectedValue");

    QTest::newRow("with group, use primary module group")
        << true << "module_group"
        << QStringList{"FEATURE_ENABLED", "THE_GROUP", "BASEDEF", "MODULE_DEFINE", "MODULE_GROUP"};
    QTest::newRow("with group, use alternative module group")
        << true << "module_group_alt"
        << QStringList{
               "FEATURE_ENABLED", "THE_GROUP", "BASEDEF", "MODULE_DEFINE", "MODULE_GROUP_ALT"};
    QTest::newRow("without group, use primary module group")
        << false << "module_group" << QStringList{"BASEDEF", "MODULE_DEFINE", "MODULE_GROUP"};
    QTest::newRow("without group, use alternative module group")
        << false << "module_group_alt"
        << QStringList{"BASEDEF", "MODULE_DEFINE", "MODULE_GROUP_ALT"};
}

void TestLanguage::propertiesBlockInGroup()
{
    QFETCH(bool, withGroup);
    QFETCH(QString, moduleGroup);
    QFETCH(QStringList, expectedValue);

    bool exceptionCaught = false;
    try {
        defaultParameters.setOverriddenValues(
            {std::make_pair(QString("products.in-group.featureEnabled"), withGroup),
             std::make_pair(QString("modules.module_with_group.group"), moduleGroup)});
        resolveProject("properties-block-in-group.qbs");
        QVERIFY(!!project);
        QCOMPARE(project->allProducts().size(), size_t(1));
        const ResolvedProductConstPtr product = project->allProducts().front();
        const QStringList productValue
            = moduleProperty(product->moduleProperties->value(), "dummy", "defines").toStringList();
        QCOMPARE(productValue, expectedValue);
        const auto groupIt = std::find_if(product->groups.cbegin(), product->groups.cend(),
                [](const GroupConstPtr &g) { return g->name == "the group"; });
        QVERIFY(groupIt != product->groups.cend());
        const QStringList groupValue
            = moduleProperty((*groupIt)->properties->value(), "dummy", "defines").toStringList();
        QCOMPARE(groupValue, QStringList{"GROUP_ONLY"});
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::propertiesItemInModule()
{
    bool exceptionCaught = false;
    try {
        resolveProject("properties-item-in-module.qbs");
        QVERIFY(!!project);
        const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 2);
        for (const ResolvedProductPtr &p : products) {
            QCOMPARE(p->moduleProperties->moduleProperty("dummy", "productName").toString(),
                     p->name);
        }
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::propertyAssignmentInExportedGroup()
{
    bool exceptionCaught = false;
    try {
        resolveProject("property-assignment-in-exported-group.qbs");
        QVERIFY(!!project);
        const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 2);
        for (const ResolvedProductPtr &p : products) {
            QCOMPARE(p->moduleProperties->moduleProperty("dummy", "someString").toString(),
                     QString());
            for (const GroupPtr &g : p->groups) {
                const QString propValue
                        = g->properties->moduleProperty("dummy", "someString").toString();
                if (g->name == "exported_group")
                    QCOMPARE(propValue, QString("test"));
                else
                    QCOMPARE(propValue, QString());
            }
        }
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::qbs1275()
{
    bool exceptionCaught = false;
    try {
        resolveProject("qbs1275.qbs");
        QVERIFY(!!project);
        const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.count(), 5);
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
        resolveProject("qbs-properties-in-project-condition.qbs");
        QVERIFY(!!project);
        QVERIFY(!project->enabled);
        const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 0);
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::qbsPropertyConvenienceOverride()
{
    bool exceptionCaught = false;
    try {
        defaultParameters.setOverriddenValues({std::make_pair("qbs.installPrefix", "/opt")});
        resolveProject("qbs-property-convenience-override.qbs");
        QVERIFY(!!project);
        QCOMPARE(project->products.size(), size_t(1));
        QCOMPARE(project->products.front()->moduleProperties->qbsPropertyValue("installPrefix")
                 .toString(), QString("/opt"));
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::relaxedErrorMode()
{
    m_logSink->setLogLevel(LoggerMinLevel);
    QFETCH(bool, strictMode);
    try {
        defaultParameters.setProductErrorMode(strictMode ? ErrorHandlingMode::Strict
                                                         : ErrorHandlingMode::Relaxed);
        resolveProject("relaxed-error-mode/relaxed-error-mode.qbs");
        QVERIFY(!strictMode);
        const auto productMap = productsFromProject(project);
        const ResolvedProductConstPtr brokenProduct = productMap.value("broken");
        QVERIFY(!brokenProduct->enabled);
        QVERIFY(brokenProduct->location.isValid());
        QCOMPARE(brokenProduct->allFiles().size(), size_t(0));
        const ResolvedProductConstPtr dependerRequired = productMap.value("depender required");
        QVERIFY(!dependerRequired->enabled);
        QVERIFY(dependerRequired->location.isValid());
        QCOMPARE(dependerRequired->allFiles().size(), size_t(1));
        const ResolvedProductConstPtr dependerNonRequired
                = productMap.value("depender nonrequired");
        QVERIFY(dependerNonRequired->enabled);
        QCOMPARE(dependerNonRequired->allFiles().size(), size_t(1));
        const ResolvedProductConstPtr recursiveDepender = productMap.value("recursive depender");
        QVERIFY(!recursiveDepender->enabled);
        QVERIFY(recursiveDepender->location.isValid());
        QCOMPARE(recursiveDepender->allFiles().size(), size_t(1));
        const ResolvedProductConstPtr missingFile = productMap.value("missing file");
        QVERIFY(missingFile->enabled);
        QCOMPARE(missingFile->groups.size(), size_t(1));
        QVERIFY(missingFile->groups.front()->enabled);
        QVERIFY(missingFile->groups.front()->files);
        QCOMPARE(missingFile->groups.front()->files->size(), size_t(2));
        const ResolvedProductConstPtr fine = productMap.value("fine");
        QVERIFY(fine->enabled);
        QCOMPARE(fine->allFiles().size(), size_t(1));
    } catch (const ErrorInfo &e) {
        QVERIFY2(strictMode, qPrintable(e.toString()));
    }
}

void TestLanguage::relaxedErrorMode_data()
{
    QTest::addColumn<bool>("strictMode");

    QTest::newRow("strict mode") << true;
    QTest::newRow("relaxed mode") << false;
}

void TestLanguage::requiredAndNonRequiredDependencies()
{
    QFETCH(QString, projectFile);
    QFETCH(bool, exceptionExpected);
    try {
        resolveProject(qPrintable("required-and-nonrequired-dependencies/" + projectFile));
        QVERIFY(!!project);
        QVERIFY(!exceptionExpected);
    } catch (const ErrorInfo &e) {
        QVERIFY(exceptionExpected);
        QVERIFY2(e.toString().contains("validation error!"), qPrintable(e.toString()));
    }
}

void TestLanguage::requiredAndNonRequiredDependencies_data()
{
    QTest::addColumn<QString>("projectFile");
    QTest::addColumn<bool>("exceptionExpected");

    QTest::newRow("same file") << "direct-dependencies.qbs" << true;
    QTest::newRow("dependency via module") << "dependency-via-module.qbs" << true;
    QTest::newRow("dependency via export") << "dependency-via-export.qbs" << true;
    QTest::newRow("more indirection") << "complicated.qbs" << true;
    QTest::newRow("required chain (module)") << "required-chain-module.qbs" << false;
    QTest::newRow("required chain (export)") << "required-chain-export.qbs" << false;
    QTest::newRow("required chain (export indirect)") << "required-chain-export-indirect.qbs"
                                                      << false;
}

void TestLanguage::suppressedAndNonSuppressedErrors()
{
    try {
        resolveProject("suppressed-and-non-suppressed-errors.qbs");
        QFAIL("failure expected");
    } catch (const ErrorInfo &e) {
        QVERIFY2(e.toString().contains("easter bunny"), qPrintable(e.toString()));
        QVERIFY2(!e.toString().contains("TheBeautifulSausage"), qPrintable(e.toString()));
    }
}

void TestLanguage::throwingProbe()
{
    QFETCH(bool, enableProbe);
    try {
        QVariantMap properties;
        properties.insert(QStringLiteral("products.theProduct.enableProbe"), enableProbe);
        defaultParameters.setOverriddenValues(properties);
        resolveProject("throwing-probe.qbs");
        QVERIFY(!!project);
        QVERIFY(!enableProbe);
    } catch (const ErrorInfo &e) {
        QVERIFY2(enableProbe, qPrintable(e.toString()));
    }
}

void TestLanguage::throwingProbe_data()
{
    QTest::addColumn<bool>("enableProbe");

    QTest::newRow("enabled probe") << true;
    QTest::newRow("disabled probe") << false;
}

void TestLanguage::qualifiedId()
{
    QString str = "foo.bar.baz";
    QualifiedId id = QualifiedId::fromString(str);
    QCOMPARE(id.size(), 3);
    QCOMPARE(id.toString(), str);

    id = QualifiedId("blubb.di.blubb"); // c'tor does not split
    QCOMPARE(id.size(), 1);

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
        resolveProject("recursive-dependencies/recursive-dependencies.qbs");
        QVERIFY(!!project);
        const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        QCOMPARE(products.size(), 4);
        const ResolvedProductConstPtr p1 = products.value("p1");
        QVERIFY(!!p1);
        const ResolvedProductConstPtr p2 = products.value("p2");
        QVERIFY(!!p2);
        const ResolvedProductPtr p3 = products.value("p3");
        QVERIFY(!!p3);
        const ResolvedProductPtr p4 = products.value("p4");
        QVERIFY(!!p4);
        QVERIFY(p1->dependencies == std::vector<ResolvedProductPtr>({p3, p4}));
        QVERIFY(p2->dependencies == std::vector<ResolvedProductPtr>({p3, p4}));
    } catch (const ErrorInfo &e) {
        exceptionCaught = true;
        qDebug() << e.toString();
    }
    QCOMPARE(exceptionCaught, false);
}

void TestLanguage::fileTags_data()
{
    QTest::addColumn<size_t>("numberOfGroups");
    QTest::addColumn<QStringList>("expectedFileTags");

    QTest::newRow("init") << size_t(0) << QStringList();
    QTest::newRow("filetagger_project_scope") << size_t(1) << (QStringList() << "cpp");
    QTest::newRow("filetagger_product_scope") << size_t(1) << (QStringList() << "asm");
    QTest::newRow("filetagger_static_pattern") << size_t(1) << (QStringList() << "yellow");
    QTest::newRow("unknown_file_tag") << size_t(1) << (QStringList() << "unknown-file-tag");
    QTest::newRow("set_file_tag_via_group") << size_t(2) << (QStringList() << "c++");
    QTest::newRow("override_file_tag_via_group") << size_t(2) << (QStringList() << "c++");
    QTest::newRow("add_file_tag_via_group") << size_t(2) << (QStringList() << "cpp" << "zzz");
    QTest::newRow("prioritized_filetagger") << size_t(1) << (QStringList() << "cpp1" << "cpp2");
    QTest::newRow("cleanup") << size_t(0) << QStringList();
}

void TestLanguage::fileTags()
{
    HANDLE_INIT_CLEANUP_DATATAGS("filetags.qbs");
    QFETCH(size_t, numberOfGroups);
    QFETCH(QStringList, expectedFileTags);
    QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
    ResolvedProductPtr product;
    const QString productName = QString::fromLocal8Bit(QTest::currentDataTag());
    QVERIFY(!!(product = products.value(productName)));
    QCOMPARE(product->groups.size(), numberOfGroups);
    GroupPtr group = *(product->groups.end() - 1);
    QVERIFY(!!group);
    QVERIFY(group->files);
    QCOMPARE(group->files->size(), size_t(1));
    SourceArtifactConstPtr sourceFile = group->files->front();
    QStringList fileTags = sourceFile->fileTags.toStringList();
    fileTags.sort();
    QCOMPARE(fileTags, expectedFileTags);
}

void TestLanguage::useInternalProfile()
{
    const QString profile(QStringLiteral("theprofile"));
    defaultParameters.setTopLevelProfile(profile);
    resolveProject("use-internal-profile.qbs");
    QVERIFY(!!project);
    QCOMPARE(project->profile(), profile);
    QCOMPARE(project->products.size(), size_t(1));
    const ResolvedProductConstPtr product = project->products[0];
    QCOMPARE(product->profile(), profile);
    QCOMPARE(product->moduleProperties->moduleProperty("dummy", "defines").toString(), profile);
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
        using Qt::endl;
        s << "Application {" << endl
          << "  name: \"MyProduct\"" << endl;
        if (useGroup) {
            s << "  Group {" << endl
              << "     name: " << toJSLiteral(groupName) << endl;
        }
        if (!prefix.isEmpty())
            s << "  prefix: " << toJSLiteral(prefix) << endl;
        if (!patterns.empty())
            s << "  files: " << toJSLiteral(patterns) << endl;
        if (!excludePatterns.empty())
            s << "  excludeFiles: " << toJSLiteral(excludePatterns) << endl;
        if (useGroup)
            s << "  }" << endl;
        s << "}" << endl << endl;
    }

    // create files
    for (QString filePath : std::as_const(filesToCreate)) {
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
        resolveProject();
        QVERIFY(!!project);
        const QHash<QString, ResolvedProductPtr> products = productsFromProject(project);
        product = products.value("MyProduct");
        QVERIFY(!!product);
        GroupPtr group;
        if (useGroup) {
            QCOMPARE(product->groups.size(), size_t(HostOsInfo::isMacosHost() ? 4 : 2));
            for (const GroupPtr &rg : product->groups) {
                if (rg->name == groupName) {
                    group = rg;
                    break;
                }
            }
        } else {
            QCOMPARE(product->groups.size(), size_t(HostOsInfo::isMacosHost() ? 3 : 1));
            group = product->groups.front();
        }
        QVERIFY(!!group);
        QVERIFY(group->files);
        QCOMPARE(group->files->size(), expected.size()); // we assume all files are wildcards
        QVERIFY(!!group->wildcards);
        QStringList actualFilePaths;
        for (const SourceArtifactPtr &artifact : *group->files) {
            QString str = artifact->absoluteFilePath;
            int idx = str.indexOf(m_wildcardsTestDirPath);
            if (idx != -1)
                str.remove(0, idx + m_wildcardsTestDirPath.size() + 1);
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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const SettingsPtr s = settings();
    TestLanguage tl(ConsoleLogger::instance().logSink(), s.get());
    return QTest::qExec(&tl, argc, argv);
}
