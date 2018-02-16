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
        return QMap<QString, QString> { };
    const auto tools = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    return QMap<QString, QString> {
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

void TestBlackboxAndroid::android()
{
    QFETCH(QString, projectDir);
    QFETCH(QStringList, productNames);
    QFETCH(QList<QByteArrayList>, expectedFilesLists);

    const SettingsPtr s = settings();
    Profile p(profileName(), s.get());
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

    QDir::setCurrent(testDataDir + "/" + projectDir);

    static const QStringList configNames { "debug", "release" };
    for (const QString &configName : configNames) {
        auto currentExpectedFilesLists = expectedFilesLists;
        QbsRunParameters params(QStringList { "--command-echo-mode", "command-line",
                                              "modules.Android.ndk.platform:android-21",
                                              "config:" + configName });
        params.profile = p.name();
        QCOMPARE(runQbs(params), 0);
        for (const QString &productName : qAsConst(productNames)) {
            QVERIFY(m_qbsStdout.contains(productName.toLocal8Bit() + ".apk"));
            const QString apkFilePath = relativeProductBuildDir(productName, configName)
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
                if (expectedFile.endsWith("/gdbserver") && configName == "release")
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
                if (none_of(actualFiles, isFileSharedObject))
                    QWARN(msg);
                else
                    QFAIL(msg);
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
    QStringList archsStringList = p.value(QLatin1String("qbs.architectures")).toStringList();
    if (archsStringList.empty())
        archsStringList << QStringLiteral("armv7a"); // must match default in common.qbs
    QByteArrayList archs;
    std::transform(archsStringList.begin(), archsStringList.end(), std::back_inserter(archs),
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
        "META-INF/MANIFEST.MF", "classes.dex", "resources.arsc"
    });

    QTest::addColumn<QString>("projectDir");
    QTest::addColumn<QStringList>("productNames");
    QTest::addColumn<QList<QByteArrayList>>("expectedFilesLists");
    QTest::newRow("teapot")
            << "teapot" << QStringList("com.sample.teapot")
            << (QList<QByteArrayList>() << commonFiles + expandArchs(archs, {
                       "assets/Shaders/ShaderPlain.fsh",
                       "assets/Shaders/VS_ShaderPlain.vsh",
                       "lib/${ARCH}/gdbserver",
                       "lib/${ARCH}/libgnustl_shared.so",
                       "lib/${ARCH}/libTeapotNativeActivity.so",
                       "res/layout/widgets.xml"}));
    QTest::newRow("no native")
            << "no-native"
            << QStringList("com.example.android.basicmediadecoder")
            << (QList<QByteArrayList>() << commonFiles + expandArchs(archs, {
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
                       "res/raw/vid_bigbuckbunny.mp4"}));
    QTest::newRow("multiple libs")
            << "multiple-libs-per-apk"
            << QStringList("twolibs")
            << (QList<QByteArrayList>() << commonFiles + expandArchs(archs, {
                       "lib/${ARCH}/gdbserver",
                       "lib/${ARCH}/liblib1.so",
                       "lib/${ARCH}/liblib2.so",
                       "lib/${ARCH}/libstlport_shared.so"}));
    QByteArrayList expectedFiles1 = (commonFiles
            + expandArchs(QByteArrayList{"mips", "x86"}, {
                              "lib/${ARCH}/gdbserver",
                              "lib/${ARCH}/libp1lib1.so",
                              "lib/${ARCH}/libstlport_shared.so"})
            + expandArchs(QByteArrayList{archs}, {
                              "lib/${ARCH}/gdbserver",
                              "lib/${ARCH}/libp1lib2.so",
                              "lib/${ARCH}/libstlport_shared.so"})).toSet().toList();
    QByteArrayList expectedFiles2 = commonFiles + expandArchs(archs, {
                       "lib/${ARCH}/gdbserver",
                       "lib/${ARCH}/libp2lib1.so",
                       "lib/${ARCH}/libp2lib2.so",
                       "lib/${ARCH}/libstlport_shared.so"});
    expectedFiles2.removeOne("resources.arsc");
    QTest::newRow("multiple apks")
            << "multiple-apks-per-project"
            << (QStringList() << "twolibs1" << "twolibs2")
            << QList<QByteArrayList>{expectedFiles1, expectedFiles2};
}

QTEST_MAIN(TestBlackboxAndroid)
