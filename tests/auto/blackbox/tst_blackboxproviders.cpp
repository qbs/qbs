/****************************************************************************
**
** Copyright (C) 2023 Ivan Komissarov (abbapoh@gmail.com)
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
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

#include "tst_blackboxproviders.h"

#include "../shared.h"

// #include <tools/hostosinfo.h>
// #include <tools/profile.h>
// #include <tools/qttools.h>

// #include <QtCore/qdir.h>
// #include <QtCore/qregularexpression.h>

// using qbs::Internal::HostOsInfo;
// using qbs::Profile;

#define WAIT_FOR_NEW_TIMESTAMP() waitForNewTimestamp(testDataDir)

TestBlackboxProviders::TestBlackboxProviders()
    : TestBlackboxBase(SRCDIR "/testdata-providers", "blackbox-providers")
{
}

void TestBlackboxProviders::brokenProvider()
{
    QDir::setCurrent(testDataDir + "/broken-provider");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);

    QVERIFY(m_qbsStderr.contains("Error executing provider for module 'qbsothermodule'"));
    QVERIFY(m_qbsStderr.contains("Error executing provider for module 'qbsmetatestmodule'"));
    QCOMPARE(m_qbsStderr.count("This provider is broken"), 2);
}

void TestBlackboxProviders::fallbackModuleProvider_data()
{
    QTest::addColumn<bool>("fallbacksEnabledGlobally");
    QTest::addColumn<bool>("fallbacksEnabledInProduct");
    QTest::addColumn<QStringList>("pkgConfigLibDirs");
    QTest::addColumn<bool>("successExpected");
    QTest::newRow("without custom lib dir, fallbacks disabled globally and in product")
            << false << false << QStringList() << false;
    QTest::newRow("without custom lib dir, fallbacks disabled globally, enabled in product")
            << false << true << QStringList() << false;
    QTest::newRow("without custom lib dir, fallbacks enabled globally, disabled in product")
            << true << false << QStringList() << false;
    QTest::newRow("without custom lib dir, fallbacks enabled globally and in product")
            << true << true << QStringList() << false;
    QTest::newRow("with custom lib dir, fallbacks disabled globally and in product")
            << false << false << QStringList(testDataDir + "/fallback-module-provider/libdir")
            << false;
    QTest::newRow("with custom lib dir, fallbacks disabled globally, enabled in product")
            << false << true << QStringList(testDataDir + "/fallback-module-provider/libdir")
            << false;
    QTest::newRow("with custom lib dir, fallbacks enabled globally, disabled in product")
            << true << false << QStringList(testDataDir + "/fallback-module-provider/libdir")
            << false;
    QTest::newRow("with custom lib dir, fallbacks enabled globally and in product")
            << true << true << QStringList(testDataDir + "/fallback-module-provider/libdir")
            << true;
}

void TestBlackboxProviders::fallbackModuleProvider()
{
    QFETCH(bool, fallbacksEnabledInProduct);
    QFETCH(bool, fallbacksEnabledGlobally);
    QFETCH(QStringList, pkgConfigLibDirs);
    QFETCH(bool, successExpected);

    QDir::setCurrent(testDataDir + "/fallback-module-provider");
    static const auto b2s = [](bool b) { return QString(b ? "true" : "false"); };
    QbsRunParameters resolveParams("resolve",
        QStringList{"modules.pkgconfig.libDirs:" + pkgConfigLibDirs.join(','),
                    "products.p.fallbacksEnabled:" + b2s(fallbacksEnabledInProduct),
                    "--force-probe-execution"});
    if (!fallbacksEnabledGlobally)
        resolveParams.arguments << "--no-fallback-module-provider";
    QCOMPARE(runQbs(resolveParams), 0);
    const bool pkgConfigPresent = m_qbsStdout.contains("pkg-config present: true");
    const bool pkgConfigNotPresent = m_qbsStdout.contains("pkg-config present: false");
    QVERIFY(pkgConfigPresent != pkgConfigNotPresent);
    if (pkgConfigNotPresent)
        successExpected = false;
    QbsRunParameters buildParams;
    buildParams.expectFailure = !successExpected;
    QCOMPARE(runQbs(buildParams) == 0, successExpected);
}

void TestBlackboxProviders::moduleProviders()
{
    QDir::setCurrent(testDataDir + "/module-providers");

    // Resolving in dry-run mode must not leave any data behind.
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList("-n"))), 0);
    if (m_qbsStdout.contains("targetPlatform differs from hostPlatform"))
        QSKIP("Cannot run binaries in cross-compiled build");
    QCOMPARE(m_qbsStdout.count("Running setup script for mygenerator"), 2);
    QVERIFY(!QFile::exists(relativeBuildDir()));

    // Initial build.
    QCOMPARE(runQbs(QbsRunParameters("run", QStringList{"-p", "app1"})), 0);
    QVERIFY(QFile::exists(relativeBuildDir()));
    QCOMPARE(m_qbsStdout.count("Running setup script for mygenerator"), 2);
    QVERIFY2(m_qbsStdout.contains("The letters are A and B"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("The MY_DEFINE is app1"), m_qbsStdout.constData());
    QCOMPARE(runQbs(QbsRunParameters("run", QStringList{"-p", "app2"})), 0);
    QVERIFY2(m_qbsStdout.contains("The letters are Z and Y"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("The MY_DEFINE is app2"), m_qbsStdout.constData());

    // Rebuild with overridden module provider config. The output for product 2 must change,
    // but no setup script must be re-run, because both config values have already been
    // handled in the first run.
    const QStringList resolveArgs("moduleProviders.mygenerator.chooseLettersFrom:beginning");
    QCOMPARE(runQbs(QbsRunParameters("resolve", resolveArgs)), 0);
    QVERIFY2(!m_qbsStdout.contains("Running setup script"), m_qbsStdout.constData());
    QCOMPARE(runQbs(QbsRunParameters("run", QStringList{"-p", "app1"})), 0);
    QVERIFY2(m_qbsStdout.contains("The letters are A and B"), m_qbsStdout.constData());
    QCOMPARE(runQbs(QbsRunParameters("run", QStringList{"-p", "app2"})), 0);
    QVERIFY2(m_qbsStdout.contains("The letters are A and B"), m_qbsStdout.constData());

    // Forcing Probe execution triggers a re-run of the setup script. But only once,
    // because the module provider config is the same now.
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList(resolveArgs)
                                     << "--force-probe-execution")), 0);
    QCOMPARE(m_qbsStdout.count("Running setup script for mygenerator"), 1);
    QCOMPARE(runQbs(QbsRunParameters("run", QStringList{"-p", "app1"})), 0);
    QVERIFY2(m_qbsStdout.contains("The letters are A and B"), m_qbsStdout.constData());
    QCOMPARE(runQbs(QbsRunParameters("run", QStringList{"-p", "app2"})), 0);
    QVERIFY2(m_qbsStdout.contains("The letters are A and B"), m_qbsStdout.constData());

    // Now re-run without the module provider config override. Again, the setup script must
    // run once, for the config value that was not present in the last run.
    QCOMPARE(runQbs(QbsRunParameters("resolve")), 0);
    QCOMPARE(m_qbsStdout.count("Running setup script for mygenerator"), 1);
    QCOMPARE(runQbs(QbsRunParameters("run", QStringList{"-p", "app1"})), 0);
    QVERIFY2(m_qbsStdout.contains("The letters are A and B"), m_qbsStdout.constData());
    QCOMPARE(runQbs(QbsRunParameters("run", QStringList{"-p", "app2"})), 0);
    QVERIFY2(m_qbsStdout.contains("The letters are Z and Y"), m_qbsStdout.constData());
}

// Checks regression - when loading 2 modules from the same provider, the second module should
// come from provider cache
void TestBlackboxProviders::moduleProvidersCache()
{
    QDir::setCurrent(testDataDir + "/module-providers-cache");

    QbsRunParameters params("resolve", {"-v"});
    QCOMPARE(runQbs(params), 0);
    const auto qbsmetatestmoduleMessage = "Re-checking for module \"qbsmetatestmodule\" with "
                                          "newly added search paths from module provider";
    const auto qbsothermoduleMessage = "Re-checking for module \"qbsothermodule\" with "
                                       "newly added search paths from module provider";
    QCOMPARE(m_qbsStderr.count(qbsmetatestmoduleMessage), 1);
    QCOMPARE(m_qbsStderr.count(qbsothermoduleMessage), 1);
    QCOMPARE(m_qbsStderr.count("Re-using provider \"provider_a\" from cache"), 1);

    // We didn't change providers, so both modules should come from cache.
    params.arguments << "project.dummyProp:value";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(m_qbsStderr.count(qbsmetatestmoduleMessage), 1);
    QCOMPARE(m_qbsStderr.count(qbsothermoduleMessage), 1);
    QCOMPARE(m_qbsStderr.count("Re-using provider \"provider_a\" from cache"), 2);
}

void TestBlackboxProviders::nonEagerModuleProvider()
{
    QDir::setCurrent(testDataDir + "/non-eager-provider");

    QbsRunParameters params("resolve");
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains(("Running setup script for qbsmetatestmodule")), m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains(("Running setup script for qbsothermodule")), m_qbsStdout);
    QVERIFY2(!m_qbsStdout.contains(("Running setup script for nonexistentmodule")), m_qbsStdout);

    QVERIFY2(m_qbsStdout.contains(("p1.qbsmetatestmodule.prop: from_provider_a")),
             m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains(("p1.qbsothermodule.prop: from_provider_a")),
             m_qbsStdout);
}

void TestBlackboxProviders::probeInModuleProvider()
{
    QDir::setCurrent(testDataDir + "/probe-in-module-provider");

    QbsRunParameters params;
    params.command = "build";
    params.arguments << "--force-probe-execution";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("Running probe"), m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains("p.qbsmetatestmodule.boolProp: true"), m_qbsStdout);
    WAIT_FOR_NEW_TIMESTAMP();
    touch("probe-in-module-provider.qbs");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("p.qbsmetatestmodule.boolProp: true"), m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains("p.qbsmetatestmodule.prop: \"value\""), m_qbsStdout);
    QVERIFY2(!m_qbsStdout.contains("Running probe"), m_qbsStdout);
}

// Tests whether it is possible to set providers properties in a Product or from command-line
void TestBlackboxProviders::providersProperties()
{
    QDir::setCurrent(testDataDir + "/providers-properties");

    QbsRunParameters params("build");
    params.arguments = QStringList("moduleProviders.provider_b.someProp: \"first,second\"");
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("p.qbsmetatestmodule.listProp: [\"someValue\"]"), m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains(
            "p.qbsothermodule.listProp: [\"first\",\"second\"]"), m_qbsStdout);
}

// checks that we can set qbs module properties in providers and provider cache works corectly
void TestBlackboxProviders::qbsModulePropertiesInProviders()
{
    QDir::setCurrent(testDataDir + "/qbs-module-properties-in-providers");

    QbsRunParameters params("resolve");

    QCOMPARE(runQbs(params), 0);

    // We have 2 products in 2 configurations, but second product should use the cached value
    // so we should have only 2 copies of the module, not 4.
    QCOMPARE(m_qbsStdout.count("Running setup script for qbsmetatestmodule"), 2);

    // Check that products get correct values from modules
    QVERIFY2(m_qbsStdout.contains(("product1.qbsmetatestmodule.prop: /sysroot1")), m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains(("product1.qbsmetatestmodule.prop: /sysroot2")), m_qbsStdout);

    QVERIFY2(m_qbsStdout.contains(("product2.qbsmetatestmodule.prop: /sysroot1")), m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains(("product2.qbsmetatestmodule.prop: /sysroot2")), m_qbsStdout);
}

void TestBlackboxProviders::qbsModuleProviders_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<QString>("firstProp");
    QTest::addColumn<QString>("secondProp");

    QTest::newRow("default") << QStringList() << "from_provider_a" << "undefined";
    QTest::newRow("override")
            << QStringList("projects.project.qbsModuleProviders:provider_b")
            << "from_provider_b"
            << "from_provider_b";
    QTest::newRow("override list a")
            << QStringList("projects.project.qbsModuleProviders:provider_a,provider_b")
            << "from_provider_a"
            << "from_provider_b";
    QTest::newRow("override list b")
            << QStringList("projects.project.qbsModuleProviders:provider_b,provider_a")
            << "from_provider_b"
            << "from_provider_b";
}

// Tests whether it is possible to set qbsModuleProviders in Product and Project items
// and that the order of providers results in correct priority
void TestBlackboxProviders::qbsModuleProviders()
{
    QFETCH(QStringList, arguments);
    QFETCH(QString, firstProp);
    QFETCH(QString, secondProp);

    QDir::setCurrent(testDataDir + "/qbs-module-providers");

    QbsRunParameters params("resolve");
    params.arguments = arguments;
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains(("p1.qbsmetatestmodule.prop: " + firstProp).toUtf8()),
             m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains(("p1.qbsothermodule.prop: " + secondProp).toUtf8()),
             m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains(("p2.qbsmetatestmodule.prop: " + firstProp).toUtf8()),
             m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains(("p2.qbsothermodule.prop: " + secondProp).toUtf8()),
             m_qbsStdout);
}

void TestBlackboxProviders::qbsModuleProvidersCliOverride_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<QString>("propertyValue");

    QTest::newRow("default") << QStringList() << "undefined";
    QTest::newRow("project-wide")
            << QStringList("project.qbsModuleProviders:provider_a")
            << "from_provider_a";
    QTest::newRow("concrete project")
            << QStringList("projects.innerProject.qbsModuleProviders:provider_a")
            << "from_provider_a";
    QTest::newRow("concrete product")
            << QStringList("products.product.qbsModuleProviders:provider_a")
            << "from_provider_a";
    QTest::newRow("concrete project override project-wide")
            << QStringList({
                    "project.qbsModuleProviders:provider_a",
                    "projects.innerProject.qbsModuleProviders:provider_b"})
            << "from_provider_b";
    QTest::newRow("concrete product override project-wide")
            << QStringList({
                    "project.qbsModuleProviders:provider_a",
                    "products.product.qbsModuleProviders:provider_b"})
            << "from_provider_b";
}

// Tests possible use-cases how to override providers from command-line
void TestBlackboxProviders::qbsModuleProvidersCliOverride()
{
    QFETCH(QStringList, arguments);
    QFETCH(QString, propertyValue);

    QDir::setCurrent(testDataDir + "/qbs-module-providers-cli-override");

    QbsRunParameters params("resolve");
    params.arguments = arguments;
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains(("qbsmetatestmodule.prop: " + propertyValue).toUtf8()),
             m_qbsStdout);
}

void TestBlackboxProviders::qbsModuleProvidersCompatibility_data()
{
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<QString>("propertyValue");

    QTest::newRow("default") << QStringList() << "from_scoped_provider";
    QTest::newRow("scoped by name") << QStringList("project.qbsModuleProviders:qbsmetatestmodule") << "from_scoped_provider";
    QTest::newRow("named") << QStringList("project.qbsModuleProviders:named_provider") << "from_named_provider";
}

// Tests whether scoped providers can be used as named, i.e. new provider machinery
// is compatible with the old one
void TestBlackboxProviders::qbsModuleProvidersCompatibility()
{
    QFETCH(QStringList, arguments);
    QFETCH(QString, propertyValue);

    QDir::setCurrent(testDataDir + "/qbs-module-providers-compatibility");

    QbsRunParameters params("resolve");
    params.arguments = arguments;
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains(("qbsmetatestmodule.prop: " + propertyValue).toUtf8()),
             m_qbsStdout);
}

void TestBlackboxProviders::qbspkgconfigModuleProvider()
{
    QDir::setCurrent(testDataDir + "/qbspkgconfig-module-provider/libs");
    rmDirR(relativeBuildDir());

    const auto commonParams = QbsRunParameters(QStringLiteral("install"), {
            QStringLiteral("--install-root"),
            QStringLiteral("install-root")
    });
    auto dynamicParams = commonParams;
    dynamicParams.arguments << "config:library" << "projects.libs.isBundle:false";
    QCOMPARE(runQbs(dynamicParams), 0);

    QDir::setCurrent(testDataDir + "/qbspkgconfig-module-provider");
    rmDirR(relativeBuildDir());

    const auto sysroot = testDataDir + "/qbspkgconfig-module-provider/libs/install-root";

    QbsRunParameters params;
    params.arguments << "moduleProviders.qbspkgconfig.sysroot:" + sysroot;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackboxProviders::removalVersion()
{
    QDir::setCurrent(testDataDir + "/removal-version");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStderr.contains(
        "Property 'deprecated' was scheduled for removal in version 2.2.0, but is still present"));
}

QTEST_MAIN(TestBlackboxProviders)
