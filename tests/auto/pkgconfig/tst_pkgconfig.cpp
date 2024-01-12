/****************************************************************************
**
** Copyright (C) 2021 Ivan Komissarov (abbapoh@gmail.com)
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "tst_pkgconfig.h"

#include "../shared.h"

#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <pkgconfig.h>
#include <pcparser.h>
#include <jsextensions/pkgconfigjs.h>

#include <QJsonArray>
#include <QJsonDocument>

using HostOsInfo = qbs::Internal::HostOsInfo;
using PcPackage = qbs::PcPackage;
using PkgConfig = qbs::PkgConfig;
using Options = qbs::PkgConfig::Options;

TestPkgConfig::TestPkgConfig()
    : m_sourceDataDir(testDataSourceDir(SRCDIR "/testdata"))
    , m_workingDataDir(testWorkDir(QStringLiteral("pkgconfig")))
{
}

void TestPkgConfig::initTestCase()
{
    QString errorMessage;
    qbs::Internal::removeDirectoryWithContents(m_workingDataDir, &errorMessage);
    QVERIFY2(qbs::Internal::copyFileRecursion(m_sourceDataDir,
                                              m_workingDataDir, false, true, &errorMessage),
             qPrintable(errorMessage));
}

void TestPkgConfig::fileName()
{
    QCOMPARE(qbs::Internal::fileName(""), "");
    QCOMPARE(qbs::Internal::fileName("file.txt"), "file.txt");
    QCOMPARE(qbs::Internal::fileName("/home/user/file.txt"), "file.txt");
    QCOMPARE(qbs::Internal::fileName("/"), "");
#if defined(Q_OS_WIN)
    QCOMPARE(qbs::Internal::fileName("c:file.txt"), "file.txt");
    QCOMPARE(qbs::Internal::fileName("c:"), "");
#endif
}

void TestPkgConfig::completeBaseName()
{
    QCOMPARE(qbs::Internal::completeBaseName(""), "");
    QCOMPARE(qbs::Internal::completeBaseName("file.txt"), "file");
    QCOMPARE(qbs::Internal::completeBaseName("archive.tar.gz"), "archive.tar");
    QCOMPARE(qbs::Internal::completeBaseName("/home/user/file.txt"), "file");
#if defined(Q_OS_WIN)
    QCOMPARE(qbs::Internal::completeBaseName("c:file.txt"), "file");
    QCOMPARE(qbs::Internal::completeBaseName("c:archive.tar.gz"), "archive.tar");
    QCOMPARE(qbs::Internal::completeBaseName("c:"), "");
#endif
}

void TestPkgConfig::parentPath()
{
    QCOMPARE(qbs::Internal::parentPath(""), "");
    QCOMPARE(qbs::Internal::parentPath("file.txt"), ".");
    QCOMPARE(qbs::Internal::parentPath("/home/user/file.txt"), "/home/user");
    QCOMPARE(qbs::Internal::parentPath("/home/user/"), "/home/user");
    QCOMPARE(qbs::Internal::parentPath("/home"), "/");
    QCOMPARE(qbs::Internal::parentPath("/"), "/");
#if defined(Q_OS_WIN)
    QCOMPARE(qbs::Internal::parentPath("c:/folder/file.txt"), "c:/folder");
    QCOMPARE(qbs::Internal::parentPath("c:/folder/"), "c:/folder");
    QCOMPARE(qbs::Internal::parentPath("c:/folder"), "c:/");
    QCOMPARE(qbs::Internal::parentPath("c:/"), "c:/");
    QCOMPARE(qbs::Internal::parentPath("c:"), "c:");
#endif
}

void TestPkgConfig::pkgConfig()
{
    QFETCH(QString, pcFileName);
    QFETCH(QString, jsonFileName);
    QFETCH(QVariantMap, optionsMap);

    if (jsonFileName.isEmpty())
        jsonFileName = pcFileName;

    if (!optionsMap.contains("mergeDependencies"))
        optionsMap["mergeDependencies"] = false;

    Options options = qbs::Internal::PkgConfigJs::convertOptions(
            QProcessEnvironment::systemEnvironment(), optionsMap);
    options.libDirs.push_back(m_workingDataDir.toStdString());

    PkgConfig pkgConfig(std::move(options));

    QFile jsonFile(m_workingDataDir + "/" + jsonFileName + ".json");
    QVERIFY(jsonFile.open(QIODevice::ReadOnly));
    QJsonParseError error{};
    const auto json = QJsonDocument::fromJson(jsonFile.readAll(), &error).toVariant().toMap();
    QCOMPARE(error.error, QJsonParseError::NoError);

    const auto &packageOr = pkgConfig.getPackage(pcFileName.toStdString());
    QVERIFY(packageOr.isValid());
    const auto &package = packageOr.asPackage();
    QCOMPARE(QString::fromStdString(package.baseFileName), pcFileName);
    QCOMPARE(QString::fromStdString(package.name), json.value("Name").toString());
    QCOMPARE(QString::fromStdString(package.description), json.value("Description").toString());
    QCOMPARE(QString::fromStdString(package.version), json.value("Version").toString());

    auto variables = json["Vars"].toMap();
    variables["pcfiledir"] = QFileInfo(m_workingDataDir).absoluteFilePath();

    QCOMPARE(size_t(variables.size()), package.variables.size());
    for (const auto &[key, value]: package.variables) {
        QCOMPARE(QString::fromStdString(value),
                 variables.value(QString::fromStdString(key)).toString());
    }

    const auto jsonLibs = json.value("Libs").toJsonArray().toVariantList();
    QCOMPARE(package.libs.size(), size_t(jsonLibs.size()));
    for (size_t i = 0; i < package.libs.size(); ++i) {
        const auto &item = package.libs[i];
        const auto jsonItem = jsonLibs.at(i).toMap();

        QCOMPARE(item.type,
                 *PcPackage::Flag::typeFromString(jsonItem.value("Type").toString().toStdString()));
        QCOMPARE(QString::fromStdString(item.value), jsonItem.value("Value").toString());
    }

    const auto jsonLibsPrivate = json.value("LibsPrivate").toJsonArray().toVariantList();
    QCOMPARE(package.libsPrivate.size(), size_t(jsonLibsPrivate.size()));
    for (size_t i = 0; i < package.libsPrivate.size(); ++i) {
        const auto &item = package.libsPrivate[i];
        const auto jsonItem = jsonLibsPrivate.at(i).toMap();

        QCOMPARE(item.type,
                 *PcPackage::Flag::typeFromString(jsonItem.value("Type").toString().toStdString()));
        QCOMPARE(QString::fromStdString(item.value), jsonItem.value("Value").toString());
    }

    const auto jsonCFlags = json.value("Cflags").toJsonArray().toVariantList();
    QCOMPARE(package.cflags.size(), size_t(jsonCFlags.size()));
    for (size_t i = 0; i < package.cflags.size(); ++i) {
        const auto &item = package.cflags[i];
        const auto jsonItem = jsonCFlags.at(i).toMap();

        QCOMPARE(item.type,
                 *PcPackage::Flag::typeFromString(jsonItem.value("Type").toString().toStdString()));
        QCOMPARE(QString::fromStdString(item.value), jsonItem.value("Value").toString());
    }

    for (const auto &item: package.requiresPublic)
        qInfo() << "requires" << item.name.c_str() << item.version.c_str();

    const auto jsonRequires = json.value("Requires").toJsonArray().toVariantList();
    QCOMPARE(package.requiresPublic.size(), size_t(jsonRequires.size()));
    for (size_t i = 0; i < package.requiresPublic.size(); ++i) {
        const auto &item = package.requiresPublic[i];
        const auto jsonItem = jsonRequires.at(i).toMap();

        QCOMPARE(item.comparison,
                 *PcPackage::RequiredVersion::comparisonFromString(
                     jsonItem.value("Comparison").toString().toStdString()));
        QCOMPARE(QString::fromStdString(item.name), jsonItem.value("Name").toString());
        QCOMPARE(QString::fromStdString(item.version), jsonItem.value("Version").toString());
    }

    const auto jsonRequiresPrivate = json.value("RequiresPrivate").toJsonArray().toVariantList();
    QCOMPARE(package.requiresPrivate.size(), size_t(jsonRequiresPrivate.size()));
    for (size_t i = 0; i < package.requiresPrivate.size(); ++i) {
        const auto &item = package.requiresPrivate[i];
        const auto jsonItem = jsonRequiresPrivate.at(i).toMap();

        QCOMPARE(item.comparison,
                 *PcPackage::RequiredVersion::comparisonFromString(
                     jsonItem.value("Comparison").toString().toStdString()));
        QCOMPARE(QString::fromStdString(item.name), jsonItem.value("Name").toString());
        QCOMPARE(QString::fromStdString(item.version), jsonItem.value("Version").toString());
    }
}

void TestPkgConfig::pkgConfig_data()
{
    QTest::addColumn<QString>("pcFileName");
    QTest::addColumn<QString>("jsonFileName");
    QTest::addColumn<QVariantMap>("optionsMap");

    QTest::newRow("empty-variable")
            << QStringLiteral("empty-variable") << QString() << QVariantMap();
    QTest::newRow("non-l-required")
            << QStringLiteral("non-l-required") << QString() << QVariantMap();
    QTest::newRow("simple")
            << QStringLiteral("simple") << QString() << QVariantMap();
    QTest::newRow("requires-test")
            << QStringLiteral("requires-test") << QString() << QVariantMap();
    QTest::newRow("requires-test-merged")
            << QStringLiteral("requires-test")
            << QStringLiteral("requires-test-merged")
            << QVariantMap({{"mergeDependencies", true}});
    QTest::newRow("requires-test-merged-static")
            << QStringLiteral("requires-test")
            << QStringLiteral("requires-test-merged-static")
            << QVariantMap({{"mergeDependencies", true}, {"staticMode", true}});
    QTest::newRow("special-flags")
            << QStringLiteral("special-flags") << QString() << QVariantMap();
    QTest::newRow("system")
            << QStringLiteral("system") << QString() << QVariantMap();
    QTest::newRow("sysroot")
            << QStringLiteral("sysroot") << QString() << QVariantMap({{"sysroot", "/newroot"}});
    QTest::newRow("tilde")
            << QStringLiteral("tilde") << QString() << QVariantMap();
    QTest::newRow("variables")
            << QStringLiteral("variables") << QString() << QVariantMap();
    QTest::newRow("variables-merged")
            << QStringLiteral("variables")
            << QString()
            << QVariantMap({{"mergeDependencies", true}});
    QTest::newRow("whitespace")
            << QStringLiteral("whitespace") << QString() << QVariantMap();
    QTest::newRow("base.name")
            << QStringLiteral("base.name") << QString() << QVariantMap();
}

void TestPkgConfig::benchSystem()
{
    if (HostOsInfo::hostOs() == HostOsInfo::HostOsWindows)
        QSKIP("Not available on Windows");
    QBENCHMARK {
        PkgConfig pkgConfig;
        QVERIFY(!pkgConfig.packages().empty());
    }
}

void TestPkgConfig::prefix()
{
    const auto prefixDir = m_workingDataDir;
    const auto libDir = m_workingDataDir + "/lib";
    const auto includeDir = m_workingDataDir + "/include";
    const auto pkgconfigDir = libDir + "/pkgconfig";
    Options options = qbs::Internal::PkgConfigJs::convertOptions(
            QProcessEnvironment::systemEnvironment(), {});
    options.definePrefix = true;
    options.libDirs.push_back(pkgconfigDir.toStdString());
    PkgConfig pkgConfig(std::move(options));
    const auto &packageOr = pkgConfig.getPackage("prefix");
    QVERIFY(packageOr.isValid());
    const auto &package = packageOr.asPackage();
    QCOMPARE(package.variables.at("prefix"), prefixDir.toStdString());
    QCOMPARE(package.variables.at("exec_prefix"), prefixDir.toStdString());
    QCOMPARE(package.variables.at("libdir"), libDir.toStdString());
    QCOMPARE(package.variables.at("includedir"), includeDir.toStdString());
    QCOMPARE(package.variables.at("usrdir"), "/usrdir");
}

QTEST_MAIN(TestPkgConfig)
