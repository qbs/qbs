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

void TestPkgConfig::pkgConfig()
{
    QFETCH(QString, fileName);
    QFETCH(QVariantMap, optionsMap);

    Options options = qbs::Internal::PkgConfigJs::convertOptions(QProcessEnvironment::systemEnvironment(), optionsMap);
    options.searchPaths.push_back(m_workingDataDir.toStdString());

    PkgConfig pkgConfig(std::move(options));

    QFile jsonFile(m_workingDataDir + "/" + fileName + ".json");
    QVERIFY(jsonFile.open(QIODevice::ReadOnly));
    QJsonParseError error{};
    const auto json = QJsonDocument::fromJson(jsonFile.readAll(), &error).toVariant().toMap();
    QCOMPARE(error.error, QJsonParseError::NoError);

    const auto &package = pkgConfig.getPackage(fileName.toStdString());
    QCOMPARE(QString::fromStdString(package.baseFileName), fileName);
    QCOMPARE(QString::fromStdString(package.name), json.value("Name").toString());
    QCOMPARE(QString::fromStdString(package.description), json.value("Description").toString());
    QCOMPARE(QString::fromStdString(package.version), json.value("Version").toString());

    auto vars = json["Vars"].toMap();
    vars["pcfiledir"] = QFileInfo(m_workingDataDir).absoluteFilePath();

    for (const auto &[key, value]: package.vars) {
        QCOMPARE(QString::fromStdString(value),
                 vars.value(QString::fromStdString(key)).toString());
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
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QVariantMap>("optionsMap");

    QTest::newRow("non-l-required") << QStringLiteral("non-l-required") << QVariantMap();
    QTest::newRow("simple") << QStringLiteral("simple") << QVariantMap();
    QTest::newRow("requires-test") << QStringLiteral("requires-test") << QVariantMap();
    QTest::newRow("special-flags") << QStringLiteral("special-flags") << QVariantMap();
    QTest::newRow("system") << QStringLiteral("system") << QVariantMap();
    QTest::newRow("sysroot")
            << QStringLiteral("sysroot") << QVariantMap({{"sysroot", "/newroot"}});
    QTest::newRow("tilde") << QStringLiteral("tilde") << QVariantMap();
    QTest::newRow("variables") << QStringLiteral("variables") << QVariantMap();
    QTest::newRow("whitespace") << QStringLiteral("whitespace") << QVariantMap();
    QTest::newRow("base.name") << QStringLiteral("base.name") << QVariantMap();
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

QTEST_MAIN(TestPkgConfig)
