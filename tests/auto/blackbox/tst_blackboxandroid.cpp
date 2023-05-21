/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "tst_blackboxandroid.h"

#include "../shared.h"
#include <tools/profile.h>
#include <tools/qttools.h>
#include <tools/stlutils.h>

#include <QtCore/qjsondocument.h>
#include <QtCore/qset.h>
#include <QtCore/qtemporarydir.h>

using qbs::Internal::none_of;
using qbs::Profile;

QMap<QString, QString> TestBlackboxAndroid::findAndroid(int *status, const QString &profile)
{
    QTemporaryDir temp;
    QDir::setCurrent(testDataDir + "/find");
    QbsRunParameters params = QStringList({"-f", "find-android.qbs", "qbs.architecture:x86"});
    params.profile = profile;
    params.buildDirectory = temp.path();
    const int res = runQbs(params);
    if (status)
        *status = res;
    QFile file(temp.path() + "/" + relativeProductBuildDir("find-android")
               + "/android.json");
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const auto tools = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    return {
        {"sdk", QDir::fromNativeSeparators(tools["sdk"].toString())},
        {"sdk-build-tools-dx", QDir::fromNativeSeparators(tools["sdk-build-tools-dx"].toString())},
        {"sdk-build-tools-d8", QDir::fromNativeSeparators(tools["sdk-build-tools-d8"].toString())},
        {"ndk", QDir::fromNativeSeparators(tools["ndk"].toString())},
        {"ndk-samples", QDir::fromNativeSeparators(tools["ndk-samples"].toString())},
        {"jar", QDir::fromNativeSeparators(tools["jar"].toString())},
    };
}

TestBlackboxAndroid::TestBlackboxAndroid()
    : TestBlackboxBase(SRCDIR "/testdata-android", "blackbox-android")
{
}

static QString theProfileName(bool forQt)
{
    return forQt ? "qbs_autotests-android-qt" : profileName();
}

void TestBlackboxAndroid::android()
{
    QFETCH(QString, projectDir);
    QFETCH(QStringList, productNames);
    QFETCH(QList<QByteArrayList>, expectedFilesLists);
    QFETCH(QStringList, customProperties);
    QFETCH(bool, enableAapt2);
    QFETCH(bool, generateAab);
    QFETCH(bool, isIncrementalBuild);
    QFETCH(bool, enableD8);

    // skip tests on github except when run in docker - this var is not propagated into the image
    if (qEnvironmentVariableIsSet("GITHUB_ACTIONS"))
        QSKIP("Skip Android tests when running on GitHub");

    const SettingsPtr s = settings();
    Profile p(theProfileName(projectDir == "qml-app" || projectDir == "qt-app"), s.get());
    if (!p.exists())
        QSKIP("Qt is not installed");
    int status;
    const auto androidPaths = findAndroid(&status, p.name());
    QCOMPARE(status, 0);

    const auto sdkPath = androidPaths["sdk"];
    if (sdkPath.isEmpty())
        QSKIP("Android SDK is not installed");

    const auto ndkPath = androidPaths["ndk"];
    if (ndkPath.isEmpty() && projectDir != "no-native")
        QSKIP("Android NDK is not installed");

    const auto ndkSamplesPath = androidPaths["ndk-samples"];
    static const QStringList ndkSamplesDirs = QStringList() << "teapot" << "no-native";
    if (!ndkPath.isEmpty() && !QFileInfo(ndkSamplesPath).isDir()
            && ndkSamplesDirs.contains(projectDir))
        QSKIP("NDK samples directory not present");

    const QString buildSubDir = enableAapt2 ? (generateAab ? "aab" : "aapt2") : "aapt";
    QDir::setCurrent(testDataDir + "/" + projectDir);
    if (!isIncrementalBuild)
        rmDirR(relativeBuildDir(buildSubDir));

    static const QStringList configNames { "debug", "release" };
    for (const QString &configName : configNames) {
        auto currentExpectedFilesLists = expectedFilesLists;
        const QString configArgument = "config:" + configName;
        QbsRunParameters resolveParams("resolve");
        resolveParams.buildDirectory = buildSubDir;
        resolveParams.arguments << configArgument << customProperties;
        resolveParams.profile = p.name();
        QCOMPARE(runQbs(resolveParams), 0);
        QbsRunParameters buildParams(QStringList{"--command-echo-mode", "command-line",
                                                 configArgument});
        buildParams.buildDirectory = buildSubDir;
        buildParams.profile = p.name();
        QCOMPARE(runQbs(buildParams), 0);
        for (const QString &productName : std::as_const(productNames)) {
            const QByteArray tag(QTest::currentDataTag());
            QCOMPARE(m_qbsStdout.count("generating BuildConfig.java"),
                     isIncrementalBuild ? 0 : productNames.size());
            const QString packageName = productName + (generateAab ? ".aab" : ".apk");
            QVERIFY(m_qbsStdout.contains(packageName.toLocal8Bit()));
            const QString packageFilePath = buildSubDir + "/" + relativeProductBuildDir(productName,
                                                                                        configName)
                    + '/' + packageName;
            QVERIFY2(regularFileExists(packageFilePath), qPrintable(packageFilePath));
            const QString jarFilePath = androidPaths["jar"];
            QVERIFY(!jarFilePath.isEmpty());
            QProcess jar;
            jar.start(jarFilePath, QStringList() << "-tf" << packageFilePath);
            QVERIFY2(jar.waitForStarted(), qPrintable(jar.errorString()));
            QVERIFY2(jar.waitForFinished(), qPrintable(jar.errorString()));
            QVERIFY2(jar.exitCode() == 0, qPrintable(jar.readAllStandardError().constData()));
            QByteArrayList actualFiles = jar.readAllStandardOutput().trimmed().split('\n');
            for (QByteArray &f : actualFiles)
                f = f.trimmed();
            QByteArrayList missingExpectedFiles;
            QByteArrayList expectedFiles = currentExpectedFilesLists.takeFirst();
            for (const QByteArray &expectedFile : expectedFiles) {
                if (expectedFile.endsWith("/libgdbserver.so") && configName == "release")
                    continue;
                auto it = std::find(actualFiles.begin(), actualFiles.end(), expectedFile);
                if (it != actualFiles.end()) {
                    actualFiles.erase(it);
                    continue;
                }
                missingExpectedFiles << expectedFile;
            }
            if (!missingExpectedFiles.empty())
                QFAIL(QByteArray("missing expected files:\n") + missingExpectedFiles.join('\n'));
            if (!actualFiles.empty()) {
                QByteArray msg = "unexpected files encountered:\n" + actualFiles.join('\n');
                auto isFileSharedObject = [](const QByteArray &f) {
                    return f.endsWith(".so");
                };
                const auto isQmlToolingLib = [](const QByteArray &f) {
                    return f.contains("qmltooling");
                };
                if (none_of(actualFiles, isFileSharedObject)
                        || qbs::Internal::all_of(actualFiles, isQmlToolingLib)) {
                    qWarning() << msg;
                } else {
                    QFAIL(msg);
                }
            }
        }

        if (projectDir == "multiple-libs-per-apk") {
            const auto dexCompilerPath = enableD8 ? androidPaths["sdk-build-tools-d8"]
                    : androidPaths["sdk-build-tools-dx"];
            QVERIFY(!dexCompilerPath.isEmpty());
            const auto lines = m_qbsStdout.split('\n');
            const auto it = std::find_if(lines.cbegin(), lines.cend(), [&](const QByteArray &line) {
                return !line.isEmpty() && line.startsWith(dexCompilerPath.toUtf8());
            });
            QVERIFY2(it != lines.cend(), qPrintable(m_qbsStdout.constData()));
            const auto line = *it;
            QVERIFY2(line.contains("lib3.jar"), qPrintable(line.constData()));
            QVERIFY2(!line.contains("lib4.jar"), qPrintable(line.constData()));
            QVERIFY2(line.contains("lib5.jar"), qPrintable(line.constData()));
            QVERIFY2(line.contains("lib6.jar"), qPrintable(line.constData()));
            QVERIFY2(!line.contains("lib7.jar"), qPrintable(line.constData()));
            QVERIFY2(line.contains("lib8.jar"), qPrintable(line.constData()));
        }
    }
}

void TestBlackboxAndroid::android_data()
{
    const SettingsPtr s = settings();
    const Profile p(profileName(), s.get());
    const Profile pQt(theProfileName(true), s.get());
    QStringList archsStringList = p.value(QStringLiteral("qbs.architectures")).toStringList();
    if (archsStringList.empty())
        archsStringList << QStringLiteral("armv7a"); // must match default in common.qbs
    QByteArrayList archs;
    std::transform(archsStringList.begin(), archsStringList.end(), std::back_inserter(archs),
                   [] (const QString &s) {
        return s.toUtf8().replace("armv7a", "armeabi-v7a")
                .replace("armv5te", "armeabi")
                .replace("arm64", "arm64-v8a");
    });
    const auto cxxLibPath = [&p, &pQt](const QByteArray &oldcxxLib, bool forQt) {
        const bool usesClang = (forQt ? pQt : p).value(QStringLiteral("qbs.toolchainType"))
                .toString() == "clang";
        const QByteArray path = "lib/${ARCH}/";
        return path + (usesClang ? QByteArrayLiteral("libc++_shared.so") : oldcxxLib);
    };
    qbs::Version version(5, 13);
    QStringList qmakeFilePaths = pQt.value(QStringLiteral("moduleProviders.Qt.qmakeFilePaths")).
            toStringList();
    if (qmakeFilePaths.size() >= 1)
        version = TestBlackboxBase::qmakeVersion(qmakeFilePaths[0]);
    bool singleArchQt = (version < qbs::Version(5, 14));
    QByteArray qtVersionMajor((version >= qbs::Version(6, 0)) ? "6" : "5");
    QByteArrayList archsForQt;
    if (singleArchQt) {
        archsForQt = { pQt.value("qbs.architecture").toString().toUtf8() };
        if (archsStringList.empty())
            archsStringList << QStringLiteral("armv7a"); // must match default in common.qbs
    } else {
        QStringList archsForQtStringList = pQt.value(QStringLiteral("qbs.architectures"))
                .toStringList();
        if (archsForQtStringList.empty())
            archsForQtStringList << pQt.value("qbs.architecture").toString();
        std::transform(archsForQtStringList.begin(),
                       archsForQtStringList.end(),
                       std::back_inserter(archsForQt),
                       [] (const QString &s) {
            return s.toUtf8();
        });
    }

    QByteArrayList ndkArchsForQt;
    std::transform(archsForQt.begin(), archsForQt.end(), std::back_inserter(ndkArchsForQt),
                   [] (const QString &s) {
        return s.toUtf8().replace("armv7a", "armeabi-v7a")
                .replace("armv5te", "armeabi")
                .replace("arm64", "arm64-v8a");
    });

    auto expandArchs = [] (const QByteArrayList &archs, const QByteArrayList &lst, bool aabPackage) {
        const QByteArray &archPlaceHolder = "${ARCH}";
        QByteArrayList result;
        QByteArray base( aabPackage ? "base/" : "");
        for (const QByteArray &entry : lst) {
            if (entry.contains(archPlaceHolder)) {
                for (const QByteArray &arch : std::as_const(archs))
                    result << (base + QByteArray(entry).replace(archPlaceHolder, arch));
            } else {
                result << (base + entry);
            }
        }
        return result;
    };

    auto commonFiles = [](bool generateAab, bool codeSign = true,
            QString keyAlias="androiddebugkey") {
        QByteArrayList files;
        if (generateAab)
            files << "base/manifest/AndroidManifest.xml" << "base/dex/classes.dex"
                  << "BundleConfig.pb";
        else
            files << "AndroidManifest.xml" << "classes.dex";
        if (codeSign)
            files << QByteArray("META-INF/" + keyAlias.toUpper().left(8).toUtf8() + ".RSA")
                  << QByteArray("META-INF/" + keyAlias.toUpper().left(8).toUtf8() + ".SF")
                  << "META-INF/MANIFEST.MF";
        return files;
    };

    QTest::addColumn<QString>("projectDir");
    QTest::addColumn<QStringList>("productNames");
    QTest::addColumn<QList<QByteArrayList>>("expectedFilesLists");
    QTest::addColumn<QStringList>("customProperties");
    QTest::addColumn<bool>("enableAapt2");
    QTest::addColumn<bool>("generateAab");
    QTest::addColumn<bool>("isIncrementalBuild");
    QTest::addColumn<bool>("enableD8");

    const auto aaptVersion = [](bool enableAapt2) {
        return QString("modules.Android.sdk.aaptName:") + (enableAapt2 ? "aapt2" : "aapt");
    };
    bool enableAapt2 = false;
    const auto packageType = [](bool generateAab) {
        return QString("modules.Android.sdk.packageType:") + (generateAab ? "aab" : "apk");
    };
    bool generateAab = false;
    bool isIncrementalBuild = false;

    const auto dexCompilerVersion = [](bool enableD8) {
        return QString("modules.Android.sdk.dexCompilerName:") + (enableD8 ? "d8" : "dx");
    };
    bool enableD8 = true;
    auto qtAppExpectedFiles = [&](bool generateAab, bool enableAapt2, bool codeSign = true,
            QString keyAlias="androiddebugkey") {
        QByteArrayList expectedFile;
        if (singleArchQt) {
            expectedFile << commonFiles(generateAab, codeSign, keyAlias) + expandArchs(ndkArchsForQt, {
                       cxxLibPath("libgnustl_shared.so", true),
                       "assets/--Added-by-androiddeployqt--/qt_cache_pregenerated_file_list",
                       "lib/${ARCH}/libplugins_imageformats_libqgif.so",
                       "lib/${ARCH}/libplugins_imageformats_libqicns.so",
                       "lib/${ARCH}/libplugins_imageformats_libqico.so",
                       "lib/${ARCH}/libplugins_imageformats_libqjpeg.so",
                       "lib/${ARCH}/libplugins_imageformats_libqtga.so",
                       "lib/${ARCH}/libplugins_imageformats_libqtiff.so",
                       "lib/${ARCH}/libplugins_imageformats_libqwbmp.so",
                       "lib/${ARCH}/libplugins_imageformats_libqwebp.so",
                       "lib/${ARCH}/libplugins_platforms_android_libqtforandroid.so",
                       "lib/${ARCH}/libplugins_styles_libqandroidstyle.so",
                       "lib/${ARCH}/libQt5Core.so",
                       "lib/${ARCH}/libQt5Gui.so",
                       "lib/${ARCH}/libQt5Widgets.so",
                       "lib/${ARCH}/libqt-app.so"}, generateAab);
        } else {
            expectedFile << commonFiles(generateAab, codeSign, keyAlias) + expandArchs(ndkArchsForQt, {
                       cxxLibPath("libgnustl_shared.so", true),
                       "lib/${ARCH}/libplugins_imageformats_qgif_${ARCH}.so",
                       "lib/${ARCH}/libplugins_imageformats_qico_${ARCH}.so",
                       "lib/${ARCH}/libplugins_imageformats_qjpeg_${ARCH}.so",
                       "lib/${ARCH}/libplugins_platforms_qtforandroid_${ARCH}.so",
                       "lib/${ARCH}/libQt" + qtVersionMajor + "Core_${ARCH}.so",
                       "lib/${ARCH}/libQt" + qtVersionMajor + "Gui_${ARCH}.so",
                       "lib/${ARCH}/libQt" + qtVersionMajor + "Widgets_${ARCH}.so",
                       "lib/${ARCH}/libqt-app_${ARCH}.so"}, generateAab);
        }
        if (generateAab)
            expectedFile << "base/resources.pb" << "base/native.pb";
        else
            expectedFile << "resources.arsc";
        if (version >= qbs::Version(5, 14))
            expectedFile << expandArchs(ndkArchsForQt, {
                       "lib/${ARCH}/libplugins_styles_qandroidstyle_${ARCH}.so"}, generateAab);
        if (version < qbs::Version(6, 0) && version >= qbs::Version(5, 14))
            expectedFile << expandArchs(ndkArchsForQt, {
                       "lib/${ARCH}/libplugins_imageformats_qicns_${ARCH}.so",
                       "lib/${ARCH}/libplugins_imageformats_qtga_${ARCH}.so",
                       "lib/${ARCH}/libplugins_imageformats_qtiff_${ARCH}.so",
                       "lib/${ARCH}/libplugins_imageformats_qwbmp_${ARCH}.so",
                       "lib/${ARCH}/libplugins_imageformats_qwebp_${ARCH}.so"}, generateAab);
        if (version >= qbs::Version(6, 5))
            expectedFile << expandArchs(ndkArchsForQt, {
                       "lib/${ARCH}/libQt6Svg_${ARCH}.so",
                       "lib/${ARCH}/libplugins_iconengines_qsvgicon_${ARCH}.so",
                       "lib/${ARCH}/libplugins_imageformats_qsvg_${ARCH}.so"}, generateAab);
        if (!enableAapt2 && version < qbs::Version(6, 0))
            expectedFile << "res/layout/splash.xml";
        return expectedFile;
    };
    auto codeSignProperties = [&](bool codeSign, QString keyStorePath, QString keystorePassword,
            QString keyPassword, QString keyAlias) {
        if (!codeSign)
            return QStringList{"modules.codesign.enableCodeSigning:false"};
        return QStringList{
            "modules.codesign.enableCodeSigning:true",
            "modules.codesign.keystorePath:" + keyStorePath,
            "modules.codesign.keystorePassword:" + keystorePassword,
            "modules.codesign.keyPassword:" + keyPassword,
            "modules.codesign.keyAlias:" + keyAlias,
        };
    };
    bool codeSign = true;
    QString keyStorePath(testDataDir + "/qt-app/test.keystore");
    QString keystorePassword("qbsKeystoreTest");
    QString keyPassword("qbsKeyTest");
    QString keyAlias("qbsTest");
    QTest::newRow("qt app")
            << "qt-app" << QStringList("qt-app")
            << (QList<QByteArrayList>() << (QByteArrayList() << qtAppExpectedFiles(generateAab,
                                                                                   enableAapt2,
                                                                                   codeSign,
                                                                                   keyAlias)))
            << (QStringList() << codeSignProperties(codeSign, keyStorePath, keystorePassword,
                                                    keyPassword, keyAlias)
                << aaptVersion(enableAapt2)
                << packageType(generateAab))
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;;
    codeSign = false;
    QTest::newRow("qt app no signing")
            << "qt-app" << QStringList("qt-app")
            << (QList<QByteArrayList>() << (QByteArrayList() << qtAppExpectedFiles(generateAab,
                                                                                   enableAapt2,
                                                                                   codeSign,
                                                                                   keyAlias)))
            << (QStringList() << codeSignProperties(codeSign, keyStorePath, keystorePassword,
                                                    keyPassword, keyAlias)
                << aaptVersion(enableAapt2)
                << packageType(generateAab))
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    enableAapt2 = true;
    codeSign = true;
    QTest::newRow("qt app aapt2")
            << "qt-app" << QStringList("qt-app")
            << (QList<QByteArrayList>() << (QByteArrayList() << qtAppExpectedFiles(generateAab,
                                                                                   enableAapt2,
                                                                                   codeSign,
                                                                                   keyAlias)))
            << (QStringList() << codeSignProperties(codeSign, keyStorePath, keystorePassword,
                                                    keyPassword, keyAlias)
                << aaptVersion(enableAapt2)
                << packageType(generateAab))
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    generateAab = true;
    QTest::newRow("qt app aab")
            << "qt-app" << QStringList("qt-app")
            << (QList<QByteArrayList>() << (QByteArrayList() << qtAppExpectedFiles(generateAab,
                                                                                   enableAapt2,
                                                                                   codeSign,
                                                                                   keyAlias)))
            << (QStringList() << codeSignProperties(codeSign, keyStorePath, keystorePassword,
                                                    keyPassword, keyAlias)
                << aaptVersion(enableAapt2)
                << packageType(generateAab))
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    codeSign = false;
    QTest::newRow("qt app aab no signing")
            << "qt-app" << QStringList("qt-app")
            << (QList<QByteArrayList>() << (QByteArrayList() << qtAppExpectedFiles(generateAab,
                                                                                   enableAapt2,
                                                                                   codeSign,
                                                                                   keyAlias)))
            << (QStringList() << codeSignProperties(codeSign, keyStorePath, keystorePassword,
                                                    keyPassword, keyAlias)
                << aaptVersion(enableAapt2)
                << packageType(generateAab))
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;

    const QByteArrayList ndkArchsForQtSave = ndkArchsForQt;
    ndkArchsForQt = {ndkArchsForQt.first()};
    QTest::newRow("qt app (single arch)")
            << "qt-app" << QStringList("qt-app")
            << (QList<QByteArrayList>() << (QByteArrayList() << qtAppExpectedFiles(generateAab,
                                                                                   enableAapt2)))
            << QStringList{aaptVersion(enableAapt2), packageType(generateAab),
                           "modules.qbs.architectures:" + archsForQt.first()}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    ndkArchsForQt = ndkArchsForQtSave;

    auto teaPotAppExpectedFiles = [&](const QByteArrayList &archs, bool generateAab) {
        QByteArrayList expectedFile;
        expectedFile << commonFiles(generateAab) + expandArchs(archs, {
                    "assets/Shaders/ShaderPlain.fsh",
                    "assets/Shaders/VS_ShaderPlain.vsh",
                    cxxLibPath("libgnustl_shared.so", false),
                    "lib/${ARCH}/libTeapotNativeActivity.so",
                    "res/layout/widgets.xml",
                    "res/mipmap-hdpi-v4/ic_launcher.png",
                    "res/mipmap-mdpi-v4/ic_launcher.png",
                    "res/mipmap-xhdpi-v4/ic_launcher.png",
                    "res/mipmap-xxhdpi-v4/ic_launcher.png"}, generateAab);
        if (generateAab)
            expectedFile << "base/resources.pb" << "base/assets.pb" << "base/native.pb";
        else
            expectedFile << "resources.arsc";
        return expectedFile;
    };
    generateAab = false;
    enableAapt2 = false;
    QTest::newRow("teapot")
            << "teapot" << QStringList("TeapotNativeActivity")
            << (QList<QByteArrayList>() << teaPotAppExpectedFiles(archs, generateAab))
            << QStringList{aaptVersion(enableAapt2), packageType(generateAab)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    enableAapt2 = true;
    QTest::newRow("teapot aapt2")
            << "teapot" << QStringList("TeapotNativeActivity")
            << (QList<QByteArrayList>() << teaPotAppExpectedFiles(archs, generateAab))
            << QStringList{aaptVersion(enableAapt2), packageType(generateAab)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    generateAab = true;
    QTest::newRow("teapot aapt2 aab")
            << "teapot" << QStringList("TeapotNativeActivity")
            << (QList<QByteArrayList>() << teaPotAppExpectedFiles(archs, generateAab))
            << QStringList{aaptVersion(enableAapt2), packageType(generateAab)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    enableAapt2 = false;
    generateAab = false;
    QTest::newRow("minimal-native")
            << "minimal-native" << QStringList("minimalnative")
            << (QList<QByteArrayList>() << commonFiles(generateAab) + expandArchs({archs.first()}, {
                       "lib/${ARCH}/libminimalnative.so",
                       cxxLibPath("libstlport_shared.so", false),
                       "lib/${ARCH}/libdependency.so"}, generateAab))
            << QStringList{"products.minimalnative.multiplexByQbsProperties:[]",
                           "modules.qbs.architecture:" + archsStringList.first(),
                           aaptVersion(enableAapt2)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    enableAapt2 = true;
    QTest::newRow("minimal-native aapt2")
            << "minimal-native" << QStringList("minimalnative")
            << (QList<QByteArrayList>() << commonFiles(generateAab) +
                (QByteArrayList() << "resources.arsc") + expandArchs({archs.first()}, {
                       "lib/${ARCH}/libminimalnative.so",
                       cxxLibPath("libstlport_shared.so", false),
                       "lib/${ARCH}/libdependency.so"}, generateAab))
            << QStringList{"products.minimalnative.multiplexByQbsProperties:[]",
                           "modules.qbs.architecture:" + archsStringList.first(),
                           aaptVersion(enableAapt2), packageType(generateAab)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    generateAab = true;
    QTest::newRow("minimal-native aapt2 aab")
            << "minimal-native" << QStringList("minimalnative")
            << (QList<QByteArrayList>() << commonFiles(generateAab) +
                (QByteArrayList() << "base/resources.pb" << "base/native.pb") +
                expandArchs({archs.first()}, {
                       "lib/${ARCH}/libminimalnative.so",
                       cxxLibPath("libstlport_shared.so", false),
                       "lib/${ARCH}/libdependency.so"}, generateAab))
            << QStringList{"products.minimalnative.multiplexByQbsProperties:[]",
                           "modules.qbs.architecture:" + archsStringList.first(),
                           aaptVersion(enableAapt2), packageType(generateAab)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    auto qmlAppExpectedFiles = [&](bool generateAab, bool enableAapt2) {
        QByteArrayList expectedFile;
        if (singleArchQt) {
            expectedFile << commonFiles(generateAab) + expandArchs(ndkArchsForQt, {
                           "assets/--Added-by-androiddeployqt--/qml/QtQuick.2/plugins.qmltypes",
                           "assets/--Added-by-androiddeployqt--/qml/QtQuick.2/qmldir",
                           "assets/--Added-by-androiddeployqt--/qml/QtQuick/Window.2/plugins.qmltypes",
                           "assets/--Added-by-androiddeployqt--/qml/QtQuick/Window.2/qmldir",
                           "assets/--Added-by-androiddeployqt--/qt_cache_pregenerated_file_list",
                           cxxLibPath("libgnustl_shared.so", true),
                           "lib/${ARCH}/libplugins_bearer_libqandroidbearer.so",
                           "lib/${ARCH}/libplugins_imageformats_libqgif.so",
                           "lib/${ARCH}/libplugins_imageformats_libqicns.so",
                           "lib/${ARCH}/libplugins_imageformats_libqico.so",
                           "lib/${ARCH}/libplugins_imageformats_libqjpeg.so",
                           "lib/${ARCH}/libplugins_imageformats_libqtga.so",
                           "lib/${ARCH}/libplugins_imageformats_libqtiff.so",
                           "lib/${ARCH}/libplugins_imageformats_libqwbmp.so",
                           "lib/${ARCH}/libplugins_imageformats_libqwebp.so",
                           "lib/${ARCH}/libplugins_platforms_android_libqtforandroid.so",
                           "lib/${ARCH}/libplugins_qmltooling_libqmldbg_debugger.so",
                           "lib/${ARCH}/libplugins_qmltooling_libqmldbg_inspector.so",
                           "lib/${ARCH}/libplugins_qmltooling_libqmldbg_local.so",
                           "lib/${ARCH}/libplugins_qmltooling_libqmldbg_messages.so",
                           "lib/${ARCH}/libplugins_qmltooling_libqmldbg_native.so",
                           "lib/${ARCH}/libplugins_qmltooling_libqmldbg_nativedebugger.so",
                           "lib/${ARCH}/libplugins_qmltooling_libqmldbg_profiler.so",
                           "lib/${ARCH}/libplugins_qmltooling_libqmldbg_preview.so",
                           "lib/${ARCH}/libplugins_qmltooling_libqmldbg_quickprofiler.so",
                           "lib/${ARCH}/libplugins_qmltooling_libqmldbg_server.so",
                           "lib/${ARCH}/libplugins_qmltooling_libqmldbg_tcp.so",
                           "lib/${ARCH}/libqml_QtQuick.2_libqtquick2plugin.so",
                           "lib/${ARCH}/libqml_QtQuick_Window.2_libwindowplugin.so",
                           "lib/${ARCH}/libQt5Core.so",
                           "lib/${ARCH}/libQt5Gui.so",
                           "lib/${ARCH}/libQt5Network.so",
                           "lib/${ARCH}/libQt5Qml.so",
                           "lib/${ARCH}/libQt5QuickParticles.so",
                           "lib/${ARCH}/libQt5Quick.so",
                           "lib/${ARCH}/libqmlapp.so"}, generateAab);
        } else {
            expectedFile << commonFiles(generateAab) + expandArchs(ndkArchsForQt, {
                           "assets/android_rcc_bundle.rcc",
                           cxxLibPath("libgnustl_shared.so", true),
                           "lib/${ARCH}/libplugins_imageformats_qgif_${ARCH}.so",
                           "lib/${ARCH}/libplugins_imageformats_qico_${ARCH}.so",
                           "lib/${ARCH}/libplugins_imageformats_qjpeg_${ARCH}.so",
                           "lib/${ARCH}/libplugins_platforms_qtforandroid_${ARCH}.so",
                           "lib/${ARCH}/libplugins_qmltooling_qmldbg_debugger_${ARCH}.so",
                           "lib/${ARCH}/libplugins_qmltooling_qmldbg_inspector_${ARCH}.so",
                           "lib/${ARCH}/libplugins_qmltooling_qmldbg_local_${ARCH}.so",
                           "lib/${ARCH}/libplugins_qmltooling_qmldbg_messages_${ARCH}.so",
                           "lib/${ARCH}/libplugins_qmltooling_qmldbg_native_${ARCH}.so",
                           "lib/${ARCH}/libplugins_qmltooling_qmldbg_nativedebugger_${ARCH}.so",
                           "lib/${ARCH}/libplugins_qmltooling_qmldbg_profiler_${ARCH}.so",
                           "lib/${ARCH}/libplugins_qmltooling_qmldbg_preview_${ARCH}.so",
                           "lib/${ARCH}/libplugins_qmltooling_qmldbg_quickprofiler_${ARCH}.so",
                           "lib/${ARCH}/libplugins_qmltooling_qmldbg_server_${ARCH}.so",
                           "lib/${ARCH}/libplugins_qmltooling_qmldbg_tcp_${ARCH}.so",
                           "lib/${ARCH}/libQt" + qtVersionMajor + "Core_${ARCH}.so",
                           "lib/${ARCH}/libQt" + qtVersionMajor + "Gui_${ARCH}.so",
                           "lib/${ARCH}/libQt" + qtVersionMajor + "Network_${ARCH}.so",
                           "lib/${ARCH}/libQt" + qtVersionMajor + "Qml_${ARCH}.so",
                           "lib/${ARCH}/libQt" + qtVersionMajor + "Quick_${ARCH}.so",
                           "lib/${ARCH}/libQt" + qtVersionMajor + "QmlModels_${ARCH}.so",
                           "lib/${ARCH}/libQt" + qtVersionMajor + "QmlWorkerScript_${ARCH}.so",
                           "lib/${ARCH}/libqmlapp_${ARCH}.so"}, generateAab);
            if (version < qbs::Version(5, 15))
                expectedFile << expandArchs(ndkArchsForQt, {
                           "lib/${ARCH}/libQt5QuickParticles_${ARCH}.so"}, generateAab);
            if (version >= qbs::Version(5, 15) && version < qbs::Version(6, 0))
                expectedFile << expandArchs(ndkArchsForQt, {
                           "lib/${ARCH}/libqml_QtQml_StateMachine_qtqmlstatemachine_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQml_WorkerScript.2_workerscriptplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQml_Models.2_modelsplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQml_qmlplugin_${ARCH}.so"}, generateAab);
            if (version >= qbs::Version(5, 14) && version < qbs::Version(6, 0))
                expectedFile << expandArchs(ndkArchsForQt, {
                           "lib/${ARCH}/libqml_QtQuick.2_qtquick2plugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Window.2_windowplugin_${ARCH}.so"},
                                            generateAab);
            if (version < qbs::Version(6, 0))
                expectedFile << expandArchs(ndkArchsForQt, {
                           "lib/${ARCH}/libplugins_bearer_qandroidbearer_${ARCH}.so",
                           "lib/${ARCH}/libplugins_imageformats_qicns_${ARCH}.so",
                           "lib/${ARCH}/libplugins_imageformats_qtga_${ARCH}.so",
                           "lib/${ARCH}/libplugins_imageformats_qtiff_${ARCH}.so",
                           "lib/${ARCH}/libplugins_imageformats_qwbmp_${ARCH}.so",
                           "lib/${ARCH}/libplugins_imageformats_qwebp_${ARCH}.so"}, generateAab);
            if (version >= qbs::Version(6, 5))
                expectedFile << expandArchs(ndkArchsForQt, {
                           "lib/${ARCH}/libQt6Svg_${ARCH}.so",
                           "lib/${ARCH}/libplugins_iconengines_qsvgicon_${ARCH}.so",
                           "lib/${ARCH}/libplugins_imageformats_qsvg_${ARCH}.so"}, generateAab);
            if (version >= qbs::Version(6, 0)) {
                expectedFile << expandArchs(ndkArchsForQt, {
                           "lib/${ARCH}/libQt6OpenGL_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQml_Models_modelsplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQml_WorkerScript_workerscriptplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_qtquick2plugin_${ARCH}.so"}, generateAab);
                if (version >= qbs::Version(6, 5))
                    expectedFile << expandArchs(ndkArchsForQt, {
                                "lib/${ARCH}/libqml_QtQml_Base_qmlplugin_${ARCH}.so",
                                "lib/${ARCH}/libqml_QtQml_qmlmetaplugin_${ARCH}.so"}, generateAab);
                else
                    expectedFile << expandArchs(ndkArchsForQt, {
                                "lib/${ARCH}/libqml_QtQml_qmlplugin_${ARCH}.so"}, generateAab);
            }
            if (version >= qbs::Version(6, 2))
                expectedFile << expandArchs(ndkArchsForQt, {
                           "lib/${ARCH}/libplugins_networkinformation_qandroidnetworkinformation_${ARCH}.so",
                           "lib/${ARCH}/libplugins_tls_qcertonlybackend_${ARCH}.so",
                           "lib/${ARCH}/libplugins_tls_qopensslbackend_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Window_quickwindowplugin_${ARCH}.so",
                                            }, generateAab);

            if (version >= qbs::Version(6, 0) && version < qbs::Version(6, 3)) {
                expectedFile << expandArchs(ndkArchsForQt, {
                           "lib/${ARCH}/libQt6QuickControls2Impl_${ARCH}.so",
                           "lib/${ARCH}/libQt6QuickControls2_${ARCH}.so",
                           "lib/${ARCH}/libQt6QuickParticles_${ARCH}.so",
                           "lib/${ARCH}/libQt6QuickShapes_${ARCH}.so",
                           "lib/${ARCH}/libQt6QuickTemplates2_${ARCH}.so",
                           "lib/${ARCH}/libQt6Sql_${ARCH}.so",
                           "lib/${ARCH}/libplugins_sqldrivers_qsqlite_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Basic_impl_qtquickcontrols2basicstyleimplplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Basic_qtquickcontrols2basicstyleplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Fusion_impl_qtquickcontrols2fusionstyleimplplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Fusion_qtquickcontrols2fusionstyleplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Imagine_impl_qtquickcontrols2imaginestyleimplplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Imagine_qtquickcontrols2imaginestyleplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Material_impl_qtquickcontrols2materialstyleimplplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Material_qtquickcontrols2materialstyleplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Universal_impl_qtquickcontrols2universalstyleimplplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Universal_qtquickcontrols2universalstyleplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_impl_qtquickcontrols2implplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_qtquickcontrols2plugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_LocalStorage_qmllocalstorageplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_NativeStyle_qtquickcontrols2nativestyleplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Particles_particlesplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Shapes_qmlshapesplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Templates_qtquicktemplates2plugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Timeline_qtquicktimelineplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Layouts_qquicklayoutsplugin_${ARCH}.so"}, generateAab);
                if (version >= qbs::Version(6, 2))
                    expectedFile << expandArchs(ndkArchsForQt, {
                                "lib/${ARCH}/libqml_QtQuick_tooling_quicktoolingplugin_${ARCH}.so",
                                "lib/${ARCH}/libQt6QmlLocalStorage_${ARCH}.so",
                                "lib/${ARCH}/libQt6QmlXmlListModel_${ARCH}.so",
                                "lib/${ARCH}/libQt6QuickDialogs2QuickImpl_${ARCH}.so",
                                "lib/${ARCH}/libQt6QuickDialogs2Utils_${ARCH}.so",
                                "lib/${ARCH}/libQt6QuickDialogs2_${ARCH}.so",
                                "lib/${ARCH}/libQt6QuickLayouts_${ARCH}.so",
                                "lib/${ARCH}/libQt6QuickTimeline_${ARCH}.so",
                                "lib/${ARCH}/libqml_QtQml_XmlListModel_qmlxmllistmodelplugin_${ARCH}.so",
                                "lib/${ARCH}/libqml_QtQuick_Dialogs_qtquickdialogsplugin_${ARCH}.so",
                                "lib/${ARCH}/libqml_QtQuick_Dialogs_quickimpl_qtquickdialogs2quickimplplugin_${ARCH}.so"},
                                            generateAab);
                else
                    expectedFile << expandArchs(ndkArchsForQt, {
                                "lib/${ARCH}/libqml_QtQuick_Window_quickwindow_${ARCH}.so",
                                "lib/${ARCH}/libqml_QtQuick_tooling_quicktooling_${ARCH}.so"}, generateAab);
            }
        }
        if (generateAab)
            expectedFile << "base/resources.pb" << "base/assets.pb" << "base/native.pb";
        else
            expectedFile << "resources.arsc";
        if (!enableAapt2 && version < qbs::Version(6, 0))
            expectedFile << "res/layout/splash.xml";
        return expectedFile;
    };
    auto qmlAppCustomMetaDataExpectedFiles = [&](bool generateAab, bool enableAapt2) {
        QByteArrayList expectedFile;
        if (singleArchQt) {
            expectedFile << commonFiles(generateAab) + expandArchs(ndkArchsForQt, {
                        "assets/--Added-by-androiddeployqt--/qml/QtQuick.2/plugins.qmltypes",
                        "assets/--Added-by-androiddeployqt--/qml/QtQuick.2/qmldir",
                        "assets/--Added-by-androiddeployqt--/qml/QtQuick/Window.2/plugins.qmltypes",
                        "assets/--Added-by-androiddeployqt--/qml/QtQuick/Window.2/qmldir",
                        "assets/--Added-by-androiddeployqt--/qt_cache_pregenerated_file_list",
                        "assets/dummyasset.txt",
                        cxxLibPath("libgnustl_shared.so", true),
                        "lib/${ARCH}/libplugins_bearer_libqandroidbearer.so",
                        "lib/${ARCH}/libplugins_imageformats_libqgif.so",
                        "lib/${ARCH}/libplugins_imageformats_libqicns.so",
                        "lib/${ARCH}/libplugins_imageformats_libqico.so",
                        "lib/${ARCH}/libplugins_imageformats_libqjpeg.so",
                        "lib/${ARCH}/libplugins_imageformats_libqtga.so",
                        "lib/${ARCH}/libplugins_imageformats_libqtiff.so",
                        "lib/${ARCH}/libplugins_imageformats_libqwbmp.so",
                        "lib/${ARCH}/libplugins_imageformats_libqwebp.so",
                        "lib/${ARCH}/libplugins_platforms_android_libqtforandroid.so",
                        "lib/${ARCH}/libplugins_qmltooling_libqmldbg_debugger.so",
                        "lib/${ARCH}/libplugins_qmltooling_libqmldbg_inspector.so",
                        "lib/${ARCH}/libplugins_qmltooling_libqmldbg_local.so",
                        "lib/${ARCH}/libplugins_qmltooling_libqmldbg_messages.so",
                        "lib/${ARCH}/libplugins_qmltooling_libqmldbg_native.so",
                        "lib/${ARCH}/libplugins_qmltooling_libqmldbg_nativedebugger.so",
                        "lib/${ARCH}/libplugins_qmltooling_libqmldbg_profiler.so",
                        "lib/${ARCH}/libplugins_qmltooling_libqmldbg_preview.so",
                        "lib/${ARCH}/libplugins_qmltooling_libqmldbg_quickprofiler.so",
                        "lib/${ARCH}/libplugins_qmltooling_libqmldbg_server.so",
                        "lib/${ARCH}/libplugins_qmltooling_libqmldbg_tcp.so",
                        "lib/${ARCH}/libqml_QtQuick.2_libqtquick2plugin.so",
                        "lib/${ARCH}/libqml_QtQuick_Window.2_libwindowplugin.so",
                        "lib/${ARCH}/libQt5Core.so",
                        "lib/${ARCH}/libQt5Gui.so",
                        "lib/${ARCH}/libQt5Network.so",
                        "lib/${ARCH}/libQt5Qml.so",
                        "lib/${ARCH}/libQt5QuickParticles.so",
                        "lib/${ARCH}/libQt5Quick.so",
                        "lib/${ARCH}/libqmlapp.so"}, generateAab);
        } else {
            expectedFile << commonFiles(generateAab) + expandArchs(ndkArchsForQt, {
                        "assets/android_rcc_bundle.rcc",
                        "assets/dummyasset.txt",
                        cxxLibPath("libgnustl_shared.so", true),
                        "lib/${ARCH}/libplugins_imageformats_qgif_${ARCH}.so",
                        "lib/${ARCH}/libplugins_imageformats_qico_${ARCH}.so",
                        "lib/${ARCH}/libplugins_imageformats_qjpeg_${ARCH}.so",
                        "lib/${ARCH}/libplugins_platforms_qtforandroid_${ARCH}.so",
                        "lib/${ARCH}/libplugins_qmltooling_qmldbg_debugger_${ARCH}.so",
                        "lib/${ARCH}/libplugins_qmltooling_qmldbg_inspector_${ARCH}.so",
                        "lib/${ARCH}/libplugins_qmltooling_qmldbg_local_${ARCH}.so",
                        "lib/${ARCH}/libplugins_qmltooling_qmldbg_messages_${ARCH}.so",
                        "lib/${ARCH}/libplugins_qmltooling_qmldbg_native_${ARCH}.so",
                        "lib/${ARCH}/libplugins_qmltooling_qmldbg_nativedebugger_${ARCH}.so",
                        "lib/${ARCH}/libplugins_qmltooling_qmldbg_profiler_${ARCH}.so",
                        "lib/${ARCH}/libplugins_qmltooling_qmldbg_preview_${ARCH}.so",
                        "lib/${ARCH}/libplugins_qmltooling_qmldbg_quickprofiler_${ARCH}.so",
                        "lib/${ARCH}/libplugins_qmltooling_qmldbg_server_${ARCH}.so",
                        "lib/${ARCH}/libplugins_qmltooling_qmldbg_tcp_${ARCH}.so",
                        "lib/${ARCH}/libQt" + qtVersionMajor + "Core_${ARCH}.so",
                        "lib/${ARCH}/libQt" + qtVersionMajor + "Gui_${ARCH}.so",
                        "lib/${ARCH}/libQt" + qtVersionMajor + "Network_${ARCH}.so",
                        "lib/${ARCH}/libQt" + qtVersionMajor + "Qml_${ARCH}.so",
                        "lib/${ARCH}/libQt" + qtVersionMajor + "Quick_${ARCH}.so",
                        "lib/${ARCH}/libQt" + qtVersionMajor + "QmlModels_${ARCH}.so",
                        "lib/${ARCH}/libQt" + qtVersionMajor + "QmlWorkerScript_${ARCH}.so",
                        "lib/${ARCH}/libqmlapp_${ARCH}.so"}, generateAab);
            if (version < qbs::Version(5, 15))
                expectedFile << expandArchs(ndkArchsForQt, {
                           "lib/${ARCH}/libQt5QuickParticles_${ARCH}.so"}, generateAab);
            if (version >= qbs::Version(5, 15) && version < qbs::Version(6, 0))
                expectedFile << expandArchs(ndkArchsForQt, {
                           "lib/${ARCH}/libqml_QtQml_StateMachine_qtqmlstatemachine_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQml_WorkerScript.2_workerscriptplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQml_Models.2_modelsplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQml_qmlplugin_${ARCH}.so"}, generateAab);
            if (version >= qbs::Version(5, 14) && version < qbs::Version(6, 0))
                expectedFile << expandArchs(ndkArchsForQt, {
                           "lib/${ARCH}/libqml_QtQuick.2_qtquick2plugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Window.2_windowplugin_${ARCH}.so"},
                                            generateAab);
            if (version < qbs::Version(6, 0))
                expectedFile << expandArchs(ndkArchsForQt, {
                           "lib/${ARCH}/libplugins_bearer_qandroidbearer_${ARCH}.so",
                           "lib/${ARCH}/libplugins_imageformats_qicns_${ARCH}.so",
                           "lib/${ARCH}/libplugins_imageformats_qtga_${ARCH}.so",
                           "lib/${ARCH}/libplugins_imageformats_qtiff_${ARCH}.so",
                           "lib/${ARCH}/libplugins_imageformats_qwbmp_${ARCH}.so",
                           "lib/${ARCH}/libplugins_imageformats_qwebp_${ARCH}.so"}, generateAab);
            if (version >= qbs::Version(6, 5))
                expectedFile << expandArchs(ndkArchsForQt, {
                           "lib/${ARCH}/libQt6Svg_${ARCH}.so",
                           "lib/${ARCH}/libplugins_iconengines_qsvgicon_${ARCH}.so",
                           "lib/${ARCH}/libplugins_imageformats_qsvg_${ARCH}.so"}, generateAab);
            if (version >= qbs::Version(6, 0)) {
                expectedFile << expandArchs(ndkArchsForQt, {
                           "lib/${ARCH}/libQt6OpenGL_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQml_Models_modelsplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQml_WorkerScript_workerscriptplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_qtquick2plugin_${ARCH}.so"}, generateAab);
                if (version >= qbs::Version(6, 5))
                    expectedFile << expandArchs(ndkArchsForQt, {
                                "lib/${ARCH}/libqml_QtQml_Base_qmlplugin_${ARCH}.so",
                                "lib/${ARCH}/libqml_QtQml_qmlmetaplugin_${ARCH}.so"}, generateAab);
                else
                    expectedFile << expandArchs(ndkArchsForQt, {
                                "lib/${ARCH}/libqml_QtQml_qmlplugin_${ARCH}.so"}, generateAab);
            }
            if (version >= qbs::Version(6, 2))
                expectedFile << expandArchs(ndkArchsForQt, {
                           "lib/${ARCH}/libplugins_networkinformation_qandroidnetworkinformation_${ARCH}.so",
                           "lib/${ARCH}/libplugins_tls_qcertonlybackend_${ARCH}.so",
                           "lib/${ARCH}/libplugins_tls_qopensslbackend_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Window_quickwindowplugin_${ARCH}.so",
                                            }, generateAab);
            if (version >= qbs::Version(6, 0) && version < qbs::Version(6, 3)) {
                expectedFile << expandArchs(ndkArchsForQt, {
                           "lib/${ARCH}/libQt6QuickControls2Impl_${ARCH}.so",
                           "lib/${ARCH}/libQt6QuickControls2_${ARCH}.so",
                           "lib/${ARCH}/libQt6QuickParticles_${ARCH}.so",
                           "lib/${ARCH}/libQt6QuickShapes_${ARCH}.so",
                           "lib/${ARCH}/libQt6QuickTemplates2_${ARCH}.so",
                           "lib/${ARCH}/libQt6Sql_${ARCH}.so",
                           "lib/${ARCH}/libplugins_sqldrivers_qsqlite_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Basic_impl_qtquickcontrols2basicstyleimplplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Basic_qtquickcontrols2basicstyleplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Fusion_impl_qtquickcontrols2fusionstyleimplplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Fusion_qtquickcontrols2fusionstyleplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Imagine_impl_qtquickcontrols2imaginestyleimplplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Imagine_qtquickcontrols2imaginestyleplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Material_impl_qtquickcontrols2materialstyleimplplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Material_qtquickcontrols2materialstyleplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Universal_impl_qtquickcontrols2universalstyleimplplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_Universal_qtquickcontrols2universalstyleplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_impl_qtquickcontrols2implplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Controls_qtquickcontrols2plugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_LocalStorage_qmllocalstorageplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_NativeStyle_qtquickcontrols2nativestyleplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Particles_particlesplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Shapes_qmlshapesplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Templates_qtquicktemplates2plugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Timeline_qtquicktimelineplugin_${ARCH}.so",
                           "lib/${ARCH}/libqml_QtQuick_Layouts_qquicklayoutsplugin_${ARCH}.so"}, generateAab);
                if (version >= qbs::Version(6, 2))
                    expectedFile << expandArchs(ndkArchsForQt, {
                                "lib/${ARCH}/libqml_QtQuick_tooling_quicktoolingplugin_${ARCH}.so",
                                "lib/${ARCH}/libQt6QmlLocalStorage_${ARCH}.so",
                                "lib/${ARCH}/libQt6QmlXmlListModel_${ARCH}.so",
                                "lib/${ARCH}/libQt6QuickDialogs2QuickImpl_${ARCH}.so",
                                "lib/${ARCH}/libQt6QuickDialogs2Utils_${ARCH}.so",
                                "lib/${ARCH}/libQt6QuickDialogs2_${ARCH}.so",
                                "lib/${ARCH}/libQt6QuickLayouts_${ARCH}.so",
                                "lib/${ARCH}/libQt6QuickTimeline_${ARCH}.so",
                                "lib/${ARCH}/libqml_QtQml_XmlListModel_qmlxmllistmodelplugin_${ARCH}.so",
                                "lib/${ARCH}/libqml_QtQuick_Dialogs_qtquickdialogsplugin_${ARCH}.so",
                                "lib/${ARCH}/libqml_QtQuick_Dialogs_quickimpl_qtquickdialogs2quickimplplugin_${ARCH}.so"},
                                                 generateAab);
                else
                    expectedFile << expandArchs(ndkArchsForQt, {
                               "lib/${ARCH}/libqml_QtQuick_Window_quickwindow_${ARCH}.so",
                               "lib/${ARCH}/libqml_QtQuick_tooling_quicktooling_${ARCH}.so"}, generateAab);
            }
        }
        if (generateAab)
            expectedFile << "base/resources.pb" << "base/assets.pb" << "base/native.pb";
        else
            expectedFile << "resources.arsc";
        if (!enableAapt2 && version < qbs::Version(6, 0))
            expectedFile << "res/layout/splash.xml";
        return expectedFile;
    };
    QStringList qmlAppCustomProperties;
    if (singleArchQt) {
        qmlAppCustomProperties = QStringList{"modules.Android.sdk.automaticSources:false",
                            "modules.qbs.architecture:" + archsForQt.first()};
    } else {
        qmlAppCustomProperties = QStringList{"modules.Android.sdk.automaticSources:false"};
    }
    // aapt tool for the resources works with a directory option pointing to the parent directory
    // of the resources (res).
    // The Qt.android_support module adds res/values/libs.xml (from Qt install dir). So the res from
    // Qt install res directory is added to aapt. This results in adding res/layout/splash.xml to
    // the package eventhough the file is not needed.
    // On the other hand aapt2 requires giving all the resources files.
    // Also when enabling aapt2 the resources.arsc is always created, eventhough no resources are
    // declared.
    enableAapt2 = false;
    generateAab = false;
    QTest::newRow("qml app")
            << "qml-app" << QStringList("qmlapp")
            << (QList<QByteArrayList>() << qmlAppExpectedFiles(generateAab, enableAapt2))
            << (QStringList() << qmlAppCustomProperties << aaptVersion(enableAapt2)
                              << packageType(generateAab))
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    enableAapt2 = true;
    QTest::newRow("qml app aapt2")
            << "qml-app" << QStringList("qmlapp")
            << (QList<QByteArrayList>() << qmlAppExpectedFiles(generateAab, enableAapt2))
            << (QStringList() << qmlAppCustomProperties << aaptVersion(enableAapt2)
                              << packageType(generateAab))
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    generateAab = true;
    QTest::newRow("qml app aab")
            << "qml-app" << QStringList("qmlapp")
            << (QList<QByteArrayList>() << qmlAppExpectedFiles(generateAab, enableAapt2))
            << (QStringList() << qmlAppCustomProperties << aaptVersion(enableAapt2)
                              << packageType(generateAab))
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    enableAapt2 = false;
    generateAab = false;
    QTest::newRow("qml app with custom metadata")
            << "qml-app" << QStringList("qmlapp")
            << (QList<QByteArrayList>() << (QByteArrayList()
                                            << qmlAppCustomMetaDataExpectedFiles(generateAab,
                                                                                 enableAapt2)))
            << QStringList{"modules.Android.sdk.automaticSources:true",
               aaptVersion(enableAapt2), packageType(generateAab)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    enableAapt2 = true;
    QTest::newRow("qml app with custom metadata aapt2")
            << "qml-app" << QStringList("qmlapp")
            << (QList<QByteArrayList>() << (QByteArrayList()
                                            << qmlAppCustomMetaDataExpectedFiles(generateAab,
                                                                                 enableAapt2)))
            << QStringList{"modules.Android.sdk.automaticSources:true", aaptVersion(enableAapt2),
               packageType(generateAab)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    generateAab = true;
    if (!singleArchQt) {
        QTest::newRow("qml app with custom metadata aab")
                << "qml-app" << QStringList("qmlapp")
                << (QList<QByteArrayList>() << (QByteArrayList()
                                                << qmlAppCustomMetaDataExpectedFiles(generateAab,
                                                                                     enableAapt2)))
                << QStringList{"modules.Android.sdk.automaticSources:true", aaptVersion(enableAapt2),
                   packageType(generateAab)}
                << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    }
    isIncrementalBuild = false;
    enableAapt2 = false;
    generateAab = false;
    auto noNativeExpectedFiles = [&](bool generateAab) {
        QByteArrayList expectedFile;
        expectedFile << commonFiles(generateAab) + expandArchs(archs, {
                    "res/drawable-hdpi-v4/ic_action_play_disabled.png",
                    "res/drawable-hdpi-v4/ic_action_play.png",
                    "res/drawable-hdpi-v4/ic_launcher.png",
                    "res/drawable-hdpi-v4/tile.9.png",
                    "res/drawable-mdpi-v4/ic_action_play_disabled.png",
                    "res/drawable-mdpi-v4/ic_action_play.png",
                    "res/drawable-mdpi-v4/ic_launcher.png",
                    "res/drawable/selector_play.xml",
                    "res/drawable-xhdpi-v4/ic_action_play_disabled.png",
                    "res/drawable-xhdpi-v4/ic_action_play.png",
                    "res/drawable-xhdpi-v4/ic_launcher.png",
                    "res/drawable-xxhdpi-v4/ic_launcher.png",
                    "res/layout/sample_main.xml",
                    "res/menu/action_menu.xml",
                    "res/raw/vid_bigbuckbunny.mp4"}, generateAab);
        if (generateAab)
            expectedFile << "base/resources.pb";
        else
            expectedFile << "resources.arsc";
        return expectedFile;
    };

    QTest::newRow("no native")
            << "no-native"
            << QStringList("com.example.android.basicmediadecoder")
            << (QList<QByteArrayList>() << noNativeExpectedFiles(generateAab))
            << QStringList{aaptVersion(enableAapt2), packageType(generateAab)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    enableAapt2 = true;
    QTest::newRow("no native aapt2")
            << "no-native"
            << QStringList("com.example.android.basicmediadecoder")
            << (QList<QByteArrayList>() << noNativeExpectedFiles(generateAab))
            << QStringList{aaptVersion(enableAapt2), packageType(generateAab)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    generateAab = true;
    QTest::newRow("no native aab")
            << "no-native"
            << QStringList("com.example.android.basicmediadecoder")
            << (QList<QByteArrayList>() << noNativeExpectedFiles(generateAab))
            << QStringList{aaptVersion(enableAapt2), packageType(generateAab)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    enableAapt2 = false;
    generateAab = false;
    QTest::newRow("aidl") << "aidl" << QStringList("io.qbs.aidltest")
                               << (QList<QByteArrayList>() << (QByteArrayList()
                                                               << commonFiles(generateAab)))
                               << QStringList{aaptVersion(enableAapt2), packageType(generateAab)}
                               << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    enableAapt2 = true;
    QTest::newRow("aidl aapt2") << "aidl" << QStringList("io.qbs.aidltest")
                               << (QList<QByteArrayList>() << (QByteArrayList()
                                                               << commonFiles(generateAab)
                                                               << "resources.arsc"))
                               << QStringList{aaptVersion(enableAapt2), packageType(generateAab)}
                               << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    generateAab = true;
    QTest::newRow("aidl aab") << "aidl" << QStringList("io.qbs.aidltest")
                               << (QList<QByteArrayList>() << (QByteArrayList()
                                                               << commonFiles(generateAab)
                                                               << "base/resources.pb"))
                               << QStringList{aaptVersion(enableAapt2), packageType(generateAab)}
                               << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    enableAapt2 = false;
    generateAab = false;
    enableD8 = false;
    QTest::newRow("multiple libs")
            << "multiple-libs-per-apk"
            << QStringList("twolibs")
            << (QList<QByteArrayList>() << commonFiles(generateAab) + expandArchs(archs, {
                       "resources.arsc",
                       "lib/${ARCH}/liblib1.so",
                       "lib/${ARCH}/liblib2.so",
                       cxxLibPath("libstlport_shared.so", false)}, generateAab))
            << QStringList{aaptVersion(enableAapt2), packageType(generateAab),
               dexCompilerVersion(enableD8)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    enableD8 = true;
    QTest::newRow("multiple libs with d8")
            << "multiple-libs-per-apk"
            << QStringList("twolibs")
            << (QList<QByteArrayList>() << commonFiles(generateAab) + expandArchs(archs, {
                       "resources.arsc",
                       "lib/${ARCH}/liblib1.so",
                       "lib/${ARCH}/liblib2.so",
                       cxxLibPath("libstlport_shared.so", false)}, generateAab))
            << QStringList{aaptVersion(enableAapt2), packageType(generateAab),
               dexCompilerVersion(enableD8)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    enableD8 = false;
    enableAapt2 = true;
    QTest::newRow("multiple libs aapt2")
            << "multiple-libs-per-apk"
            << QStringList("twolibs")
            << (QList<QByteArrayList>() << commonFiles(generateAab) + expandArchs(archs, {
                       "resources.arsc",
                       "lib/${ARCH}/liblib1.so",
                       "lib/${ARCH}/liblib2.so",
                       cxxLibPath("libstlport_shared.so", false)}, generateAab))
            << QStringList{aaptVersion(enableAapt2), packageType(generateAab),
               dexCompilerVersion(enableD8)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    generateAab = true;
    QTest::newRow("multiple libs aab")
            << "multiple-libs-per-apk"
            << QStringList("twolibs")
            << (QList<QByteArrayList>() << commonFiles(generateAab) + expandArchs(archs, {
                       "resources.pb", "native.pb",
                       "lib/${ARCH}/liblib1.so",
                       "lib/${ARCH}/liblib2.so",
                       cxxLibPath("libstlport_shared.so", false)}, generateAab))
            << QStringList{aaptVersion(enableAapt2), packageType(generateAab),
               dexCompilerVersion(enableD8)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;

    enableAapt2 = false;
    generateAab = false;
    auto expectedFiles1 = [&](bool generateAab) {
        QByteArrayList expectedFile = qbs::toList(qbs::toSet(commonFiles(generateAab)
                       + expandArchs(QByteArrayList{"armeabi-v7a", "x86"}, {
                                         "lib/${ARCH}/libp1lib1.so",
                                         cxxLibPath("libstlport_shared.so", false)}, generateAab)
                       + expandArchs(QByteArrayList{archs}, {
                                         "lib/${ARCH}/libp1lib2.so",
                                         cxxLibPath("libstlport_shared.so", false)}, generateAab)));
        if (generateAab)
            expectedFile << "base/resources.pb" << "base/native.pb";
        else
            expectedFile << "resources.arsc";
        return expectedFile;
    };
    auto expectedFiles2 = [&](bool generateAab) {
        QByteArrayList expectedFile = commonFiles(generateAab) + expandArchs(archs, {
                                         "lib/${ARCH}/libp2lib1.so",
                                         "lib/${ARCH}/libp2lib2.so",
                                         cxxLibPath("libstlport_shared.so", false)}, generateAab);
        return expectedFile;
    };
    QTest::newRow("multiple apks")
            << "multiple-apks-per-project"
            << (QStringList() << "twolibs1" << "twolibs2")
            << QList<QByteArrayList>{expectedFiles1(generateAab), expectedFiles2(generateAab)}
            << QStringList{aaptVersion(enableAapt2), packageType(generateAab),
               dexCompilerVersion(enableD8)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    enableAapt2 = true;
    QTest::newRow("multiple apks aapt2")
            << "multiple-apks-per-project"
            << (QStringList() << "twolibs1" << "twolibs2")
            << (QList<QByteArrayList>() << expectedFiles1(generateAab)
                << (QByteArrayList() << expectedFiles2(generateAab) << "resources.arsc"))
            << QStringList{aaptVersion(enableAapt2), packageType(generateAab),
               dexCompilerVersion(enableD8)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
    generateAab = true;
    QTest::newRow("multiple apks aab")
            << "multiple-apks-per-project"
            << (QStringList() << "twolibs1" << "twolibs2")
            << (QList<QByteArrayList>() << expectedFiles1(generateAab)
                << (QByteArrayList() << expectedFiles2(generateAab) << "base/resources.pb"
                    << "base/native.pb"))
            << QStringList{aaptVersion(enableAapt2), packageType(generateAab),
               dexCompilerVersion(enableD8)}
            << enableAapt2 << generateAab << isIncrementalBuild << enableD8;
}

QTEST_MAIN(TestBlackboxAndroid)
