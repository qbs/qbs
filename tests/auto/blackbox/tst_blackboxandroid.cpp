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
    QFETCH(QStringList, qmlAppCustomProperties);
    QFETCH(bool, enableAapt2);

    const SettingsPtr s = settings();
    Profile p(theProfileName(projectDir == "qml-app"), s.get());
    if (!p.exists())
        p = Profile("none", s.get());
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

    const QString aaptVersion = enableAapt2 ? "aapt2" : "aapt";
    QDir::setCurrent(testDataDir + "/" + projectDir);

    static const QStringList configNames { "debug", "release" };
    for (const QString &configName : configNames) {
        auto currentExpectedFilesLists = expectedFilesLists;
        const QString configArgument = "config:" + configName;
        QbsRunParameters resolveParams("resolve");
        resolveParams.buildDirectory = aaptVersion;
        resolveParams.arguments << configArgument << qmlAppCustomProperties;
        resolveParams.profile = p.name();
        QCOMPARE(runQbs(resolveParams), 0);
        QbsRunParameters buildParams(QStringList{"--command-echo-mode", "command-line",
                                                 configArgument});
        buildParams.buildDirectory = aaptVersion;
        buildParams.profile = p.name();
        QCOMPARE(runQbs(buildParams), 0);
        for (const QString &productName : qAsConst(productNames)) {
            const QByteArray tag(QTest::currentDataTag());
            const bool isIncrementalBuild = tag.startsWith("qml app") && tag != "qml app";
            QCOMPARE(m_qbsStdout.count("Generating BuildConfig.java"),
                     isIncrementalBuild ? 0 : productNames.size());
            QVERIFY(m_qbsStdout.contains(productName.toLocal8Bit() + ".apk"));
            const QString apkFilePath = aaptVersion + "/" + relativeProductBuildDir(productName,
                                                                                    configName)
                    + '/' + productName + ".apk";
            QVERIFY2(regularFileExists(apkFilePath), qPrintable(apkFilePath));
            const QString jarFilePath = androidPaths["jar"];
            QVERIFY(!jarFilePath.isEmpty());
            QProcess jar;
            jar.start(jarFilePath, QStringList() << "-tf" << apkFilePath);
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
                        || std::all_of(actualFiles.cbegin(), actualFiles.cend(), isQmlToolingLib)) {
                    QWARN(msg);
                } else {
                    QFAIL(msg);
                }
            }
        }

        if (projectDir == "multiple-libs-per-apk") {
            const auto dxPath = androidPaths["sdk-build-tools-dx"];
            QVERIFY(!dxPath.isEmpty());
            const auto lines = m_qbsStdout.split('\n');
            const auto it = std::find_if(lines.cbegin(), lines.cend(), [&](const QByteArray &line) {
                return !line.isEmpty() && line.startsWith(dxPath.toUtf8());
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
        return QByteArray("lib/${ARCH}/") + (usesClang ? "libc++_shared.so" : oldcxxLib);
    };
    bool usingOldQt = true;
    QStringList qmakeFilePaths = pQt.value(QStringLiteral("moduleProviders.Qt.qmakeFilePaths")).
            toStringList();
    if (qmakeFilePaths.size() == 1) {
        qbs::Version version = TestBlackboxBase::qmakeVersion(qmakeFilePaths[0]);
        if (version.isValid() && version >= qbs::Version(5, 14))
            usingOldQt = false;
    }

    QByteArrayList archsForQt;
    if (usingOldQt) {
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

    auto expandArchs = [] (const QByteArrayList &archs, const QByteArrayList &lst) {
        const QByteArray &archPlaceHolder = "${ARCH}";
        QByteArrayList result;
        for (const QByteArray &entry : lst) {
            if (entry.contains(archPlaceHolder)) {
                for (const QByteArray &arch : qAsConst(archs))
                    result << QByteArray(entry).replace(archPlaceHolder, arch);
            } else {
                result << entry;
            }
        }
        return result;
    };

    const QByteArrayList commonFiles = expandArchs(archs, {
        "AndroidManifest.xml", "META-INF/ANDROIDD.RSA", "META-INF/ANDROIDD.SF",
        "META-INF/MANIFEST.MF", "classes.dex"
    });

    QTest::addColumn<QString>("projectDir");
    QTest::addColumn<QStringList>("productNames");
    QTest::addColumn<QList<QByteArrayList>>("expectedFilesLists");
    QTest::addColumn<QStringList>("qmlAppCustomProperties");
    QTest::addColumn<bool>("enableAapt2");

    const auto aaptVersion = [](bool enableAapt2) {
        return QString("modules.Android.sdk.aaptName:") + (enableAapt2 ? "aapt2" : "aapt");
    };
    bool enableAapt2 = false;
    QByteArrayList teaPotAppExpectedFiles;
    teaPotAppExpectedFiles << commonFiles + expandArchs(archs, {
                   "resources.arsc",
                   "assets/Shaders/ShaderPlain.fsh",
                   "assets/Shaders/VS_ShaderPlain.vsh",
                   cxxLibPath("libgnustl_shared.so", false),
                   "lib/${ARCH}/libTeapotNativeActivity.so",
                   "res/layout/widgets.xml",
                   "res/mipmap-hdpi-v4/ic_launcher.png",
                   "res/mipmap-mdpi-v4/ic_launcher.png",
                   "res/mipmap-xhdpi-v4/ic_launcher.png",
                   "res/mipmap-xxhdpi-v4/ic_launcher.png"});
    QTest::newRow("teapot")
            << "teapot" << QStringList("TeapotNativeActivity")
            << (QList<QByteArrayList>() << teaPotAppExpectedFiles)
            << QStringList{aaptVersion(enableAapt2)} << enableAapt2;
    enableAapt2 = true;
    QTest::newRow("teapot")
            << "teapot" << QStringList("TeapotNativeActivity")
            << (QList<QByteArrayList>() << teaPotAppExpectedFiles)
            << QStringList{aaptVersion(enableAapt2)} << enableAapt2;
    enableAapt2 = false;
    QTest::newRow("minimal-native")
            << "minimal-native" << QStringList("minimalnative")
            << (QList<QByteArrayList>() << commonFiles + expandArchs({archs.first()}, {
                       "lib/${ARCH}/libminimalnative.so",
                       cxxLibPath("libstlport_shared.so", false),
                       "lib/${ARCH}/libdependency.so"}))
            << QStringList{"products.minimalnative.multiplexByQbsProperties:[]",
                           "modules.qbs.architecture:" + archsStringList.first(),
                           aaptVersion(enableAapt2)}
            << enableAapt2;
    enableAapt2 = true;
    QTest::newRow("minimal-native")
            << "minimal-native" << QStringList("minimalnative")
            << (QList<QByteArrayList>() << commonFiles + expandArchs({archs.first()}, {
                       "resources.arsc",
                       "lib/${ARCH}/libminimalnative.so",
                       cxxLibPath("libstlport_shared.so", false),
                       "lib/${ARCH}/libdependency.so"}))
            << QStringList{"products.minimalnative.multiplexByQbsProperties:[]",
                           "modules.qbs.architecture:" + archsStringList.first(),
                           aaptVersion(enableAapt2)}
            << enableAapt2;
    QByteArrayList qmlAppExpectedFiles;
    QByteArrayList qmlAppMinistroExpectedFiles;
    QByteArrayList qmlAppCustomMetaDataExpectedFiles;
    QStringList qmlAppCustomProperties;
    if (usingOldQt) {
        qmlAppExpectedFiles << commonFiles + expandArchs(ndkArchsForQt, {
                       "resources.arsc",
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
                       "lib/${ARCH}/libqmlapp.so"});
        qmlAppMinistroExpectedFiles << commonFiles + expandArchs(ndkArchsForQt, {
                        "resources.arsc",
                        "assets/--Added-by-androiddeployqt--/qt_cache_pregenerated_file_list",
                        cxxLibPath("libgnustl_shared.so", true),
                        "lib/${ARCH}/libqmlapp.so"});
        qmlAppCustomMetaDataExpectedFiles << commonFiles + expandArchs(ndkArchsForQt, {
                        "resources.arsc",
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
                        "lib/${ARCH}/libqmlapp.so"});
        qmlAppCustomProperties = QStringList{"modules.Android.sdk.automaticSources:false",
                            "modules.qbs.architecture:" + archsForQt.first()};
    } else {
        qmlAppExpectedFiles << commonFiles + expandArchs(ndkArchsForQt, {
                       "resources.arsc",
                       "assets/android_rcc_bundle.rcc",
                       cxxLibPath("libgnustl_shared.so", true),
                       "lib/${ARCH}/libplugins_bearer_qandroidbearer_${ARCH}.so",
                       "lib/${ARCH}/libplugins_imageformats_qgif_${ARCH}.so",
                       "lib/${ARCH}/libplugins_imageformats_qicns_${ARCH}.so",
                       "lib/${ARCH}/libplugins_imageformats_qico_${ARCH}.so",
                       "lib/${ARCH}/libplugins_imageformats_qjpeg_${ARCH}.so",
                       "lib/${ARCH}/libplugins_imageformats_qtga_${ARCH}.so",
                       "lib/${ARCH}/libplugins_imageformats_qtiff_${ARCH}.so",
                       "lib/${ARCH}/libplugins_imageformats_qwbmp_${ARCH}.so",
                       "lib/${ARCH}/libplugins_imageformats_qwebp_${ARCH}.so",
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
                       "lib/${ARCH}/libqml_QtQuick.2_qtquick2plugin_${ARCH}.so",
                       "lib/${ARCH}/libqml_QtQuick_Window.2_windowplugin_${ARCH}.so",
                       "lib/${ARCH}/libQt5Core_${ARCH}.so",
                       "lib/${ARCH}/libQt5Gui_${ARCH}.so",
                       "lib/${ARCH}/libQt5Network_${ARCH}.so",
                       "lib/${ARCH}/libQt5Qml_${ARCH}.so",
                       "lib/${ARCH}/libQt5QuickParticles_${ARCH}.so",
                       "lib/${ARCH}/libQt5Quick_${ARCH}.so",
                       "lib/${ARCH}/libQt5QmlModels_${ARCH}.so",
                       "lib/${ARCH}/libQt5QmlWorkerScript_${ARCH}.so",
                       "lib/${ARCH}/libqmlapp_${ARCH}.so"});
        qmlAppMinistroExpectedFiles << commonFiles + expandArchs(ndkArchsForQt, {
                        "resources.arsc",
                        "assets/android_rcc_bundle.rcc",
                        cxxLibPath("libgnustl_shared.so", true),
                        "lib/${ARCH}/libqmlapp_${ARCH}.so"});
        qmlAppCustomMetaDataExpectedFiles << commonFiles + expandArchs(ndkArchsForQt, {
                        "resources.arsc",
                        "assets/android_rcc_bundle.rcc",
                        "assets/dummyasset.txt",
                        cxxLibPath("libgnustl_shared.so", true),
                        "lib/${ARCH}/libplugins_bearer_qandroidbearer_${ARCH}.so",
                        "lib/${ARCH}/libplugins_imageformats_qgif_${ARCH}.so",
                        "lib/${ARCH}/libplugins_imageformats_qicns_${ARCH}.so",
                        "lib/${ARCH}/libplugins_imageformats_qico_${ARCH}.so",
                        "lib/${ARCH}/libplugins_imageformats_qjpeg_${ARCH}.so",
                        "lib/${ARCH}/libplugins_imageformats_qtga_${ARCH}.so",
                        "lib/${ARCH}/libplugins_imageformats_qtiff_${ARCH}.so",
                        "lib/${ARCH}/libplugins_imageformats_qwbmp_${ARCH}.so",
                        "lib/${ARCH}/libplugins_imageformats_qwebp_${ARCH}.so",
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
                        "lib/${ARCH}/libqml_QtQuick.2_qtquick2plugin_${ARCH}.so",
                        "lib/${ARCH}/libqml_QtQuick_Window.2_windowplugin_${ARCH}.so",
                        "lib/${ARCH}/libQt5Core_${ARCH}.so",
                        "lib/${ARCH}/libQt5Gui_${ARCH}.so",
                        "lib/${ARCH}/libQt5Network_${ARCH}.so",
                        "lib/${ARCH}/libQt5Qml_${ARCH}.so",
                        "lib/${ARCH}/libQt5QuickParticles_${ARCH}.so",
                        "lib/${ARCH}/libQt5Quick_${ARCH}.so",
                        "lib/${ARCH}/libQt5QmlModels_${ARCH}.so",
                        "lib/${ARCH}/libQt5QmlWorkerScript_${ARCH}.so",
                        "lib/${ARCH}/libqmlapp_${ARCH}.so"});
        qmlAppCustomProperties = QStringList{"modules.Android.sdk.automaticSources:false"};
    }
    // aapt tool for the resources works with a directory option pointing to the parent directory
    // of the resources (res).
    // The Qt.android_support module adds res/values/libs.xml (from Qt install dir). So the res from
    // Qt install res directory is added to aapt. This results in adding res/layout/splash.xml to
    // the package eventhough the file is not needed.
    // On the other hand aapt2 requires giving all the resources files.
    // Also when enabling aapt2 the resources.arsc is always created, eventhough no resources is
    // declared.
    enableAapt2 = false;
    QTest::newRow("qml app")
            << "qml-app" << QStringList("qmlapp")
            << (QList<QByteArrayList>() << (QByteArrayList() << qmlAppExpectedFiles
                                            << "res/layout/splash.xml"))
            << (QStringList() << qmlAppCustomProperties << aaptVersion(enableAapt2)) << enableAapt2;
    enableAapt2 = true;
    QTest::newRow("qml app")
            << "qml-app" << QStringList("qmlapp")
            << (QList<QByteArrayList>() << qmlAppExpectedFiles)
            << (QStringList() << qmlAppCustomProperties << aaptVersion(enableAapt2)) << enableAapt2;
    enableAapt2 = false;
    QTest::newRow("qml app using Ministro")
            << "qml-app" << QStringList("qmlapp")
            << (QList<QByteArrayList>() << (QByteArrayList() << qmlAppMinistroExpectedFiles
                                            << "res/layout/splash.xml"))
            << QStringList{"modules.Qt.android_support.useMinistro:true",
                           "modules.Android.sdk.automaticSources:false",
                           aaptVersion(enableAapt2)} << enableAapt2;
    enableAapt2 = true;
    QTest::newRow("qml app using Ministro")
            << "qml-app" << QStringList("qmlapp")
            << (QList<QByteArrayList>() << qmlAppMinistroExpectedFiles)
            << QStringList{"modules.Qt.android_support.useMinistro:true",
                           "modules.Android.sdk.automaticSources:false",
                           aaptVersion(enableAapt2)} << enableAapt2;
    enableAapt2 = false;
    QTest::newRow("qml app with custom metadata")
            << "qml-app" << QStringList("qmlapp")
            << (QList<QByteArrayList>() << (QByteArrayList() << qmlAppCustomMetaDataExpectedFiles
                                            << "res/layout/splash.xml"))
            << QStringList{"modules.Android.sdk.automaticSources:true",
                           aaptVersion(enableAapt2)} << enableAapt2;
    enableAapt2 = true;
    QTest::newRow("qml app with custom metadata")
            << "qml-app" << QStringList("qmlapp")
            << (QList<QByteArrayList>() << qmlAppCustomMetaDataExpectedFiles)
            << QStringList{"modules.Android.sdk.automaticSources:true",
                           aaptVersion(enableAapt2)} << enableAapt2;
    enableAapt2 = false;
    QTest::newRow("no native")
            << "no-native"
            << QStringList("com.example.android.basicmediadecoder")
            << (QList<QByteArrayList>() << commonFiles + expandArchs(archs, {
                       "resources.arsc",
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
                       "res/menu-v11/action_menu.xml",
                       "res/raw/vid_bigbuckbunny.mp4"}))
            << QStringList(aaptVersion(enableAapt2)) << enableAapt2;
    enableAapt2 = true;
    QTest::newRow("no native")
            << "no-native"
            << QStringList("com.example.android.basicmediadecoder")
            << (QList<QByteArrayList>() << commonFiles + expandArchs(archs, {
                       "resources.arsc",
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
                       // I have no idea why this file is generated with aapt and not with aapt2
                       //"res/menu-v11/action_menu.xml",
                       "res/raw/vid_bigbuckbunny.mp4"}))
            << QStringList(aaptVersion(enableAapt2)) << enableAapt2;
    enableAapt2 = false;
    QTest::newRow("aidl") << "aidl" << QStringList("io.qbs.aidltest")
                               << (QList<QByteArrayList>() << (QByteArrayList() << commonFiles))
                               << QStringList(aaptVersion(enableAapt2)) << enableAapt2;
    enableAapt2 = true;
    QTest::newRow("aidl") << "aidl" << QStringList("io.qbs.aidltest")
                               << (QList<QByteArrayList>() << (QByteArrayList() << commonFiles
                                                                             << "resources.arsc"))
                               << QStringList(aaptVersion(enableAapt2)) << enableAapt2;
    enableAapt2 = false;
    QTest::newRow("multiple libs")
            << "multiple-libs-per-apk"
            << QStringList("twolibs")
            << (QList<QByteArrayList>() << commonFiles + expandArchs(archs, {
                       "resources.arsc",
                       "lib/${ARCH}/liblib1.so",
                       "lib/${ARCH}/liblib2.so",
                       cxxLibPath("libstlport_shared.so", false)}))
            << QStringList(aaptVersion(enableAapt2)) << enableAapt2;
    enableAapt2 = true;
    QTest::newRow("multiple libs")
            << "multiple-libs-per-apk"
            << QStringList("twolibs")
            << (QList<QByteArrayList>() << commonFiles + expandArchs(archs, {
                       "resources.arsc",
                       "lib/${ARCH}/liblib1.so",
                       "lib/${ARCH}/liblib2.so",
                       cxxLibPath("libstlport_shared.so", false)}))
            << QStringList(aaptVersion(enableAapt2)) << enableAapt2;
    QByteArrayList expectedFiles1 = qbs::toList(qbs::toSet(commonFiles
            + expandArchs(QByteArrayList{"armeabi-v7a", "x86"}, {
                              "resources.arsc",
                              "lib/${ARCH}/libp1lib1.so",
                              cxxLibPath("libstlport_shared.so", false)})
            + expandArchs(QByteArrayList{archs}, {
                              "resources.arsc",
                              "lib/${ARCH}/libp1lib2.so",
                              cxxLibPath("libstlport_shared.so", false)})));
    QByteArrayList expectedFiles2 = commonFiles + expandArchs(archs, {
                       "lib/${ARCH}/libp2lib1.so",
                       "lib/${ARCH}/libp2lib2.so",
                       cxxLibPath("libstlport_shared.so", false)});
    enableAapt2 = false;
    QTest::newRow("multiple apks")
            << "multiple-apks-per-project"
            << (QStringList() << "twolibs1" << "twolibs2")
            << QList<QByteArrayList>{expectedFiles1, expectedFiles2}
            << QStringList(aaptVersion(enableAapt2)) << enableAapt2;
    enableAapt2 = true;
    expectedFiles2 << "resources.arsc";
    QTest::newRow("multiple apks")
            << "multiple-apks-per-project"
            << (QStringList() << "twolibs1" << "twolibs2")
            << QList<QByteArrayList>{expectedFiles1, expectedFiles2}
            << QStringList(aaptVersion(enableAapt2)) << enableAapt2;
}

QTEST_MAIN(TestBlackboxAndroid)
