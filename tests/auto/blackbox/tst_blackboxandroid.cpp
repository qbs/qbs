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

#include <QtCore/qjsondocument.h>
#include <QtCore/qtemporarydir.h>

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
    };
}

TestBlackboxAndroid::TestBlackboxAndroid()
    : TestBlackboxBase(SRCDIR "/testdata-android", "blackbox-android")
{
}

void TestBlackboxAndroid::validateTestProfile()
{
    const SettingsPtr s = settings();
    Profile p("qbs_autotests-android", s.get());
    if (!p.exists())
        QSKIP("No Android test profile");
}

void TestBlackboxAndroid::android()
{
    QFETCH(QString, projectDir);
    QFETCH(QStringList, productNames);
    QFETCH(QList<int>, apkFileCounts);

    const SettingsPtr s = settings();
    Profile p("qbs_autotests-android", s.get());
    int status;
    const auto androidPaths = findAndroid(&status, p.name());
    QCOMPARE(status, 0);

    const auto ndkPath = androidPaths["ndk"];
    const auto ndkSamplesPath = androidPaths["ndk-samples"];
    static const QStringList ndkSamplesDirs = QStringList() << "teapot" << "no-native";
    if (!ndkPath.isEmpty() && !QFileInfo(ndkSamplesPath).isDir()
            && ndkSamplesDirs.contains(projectDir))
        QSKIP("NDK samples directory not present");

    QDir::setCurrent(testDataDir + "/" + projectDir);
    QbsRunParameters params(QStringList { "--command-echo-mode", "command-line",
                                          "modules.Android.ndk.platform:android-21" });
    params.profile = p.name();
    QCOMPARE(runQbs(params), 0);
    for (int i = 0; i < productNames.count(); ++i) {
        const QString productName = productNames.at(i);
        QVERIFY(m_qbsStdout.contains(productName.toLocal8Bit() + ".apk"));
        const QString apkFilePath = relativeProductBuildDir(productName)
                + '/' + productName + ".apk";
        QVERIFY2(regularFileExists(apkFilePath), qPrintable(apkFilePath));
        const QString jarFilePath = findExecutable(QStringList("jar"));
        QVERIFY(!jarFilePath.isEmpty());
        QProcess jar;
        jar.start(jarFilePath, QStringList() << "-tf" << apkFilePath);
        QVERIFY2(jar.waitForStarted(), qPrintable(jar.errorString()));
        QVERIFY2(jar.waitForFinished(), qPrintable(jar.errorString()));
        QVERIFY2(jar.exitCode() == 0, qPrintable(jar.readAllStandardError().constData()));
        QCOMPARE(jar.readAllStandardOutput().trimmed().split('\n').count(), apkFileCounts.at(i));
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

void TestBlackboxAndroid::android_data()
{
    const SettingsPtr s = settings();
    const Profile p("qbs_autotests-android", s.get());
    const int archCount = p.value(QLatin1String("qbs.architectures")).toStringList().count();

    QTest::addColumn<QString>("projectDir");
    QTest::addColumn<QStringList>("productNames");
    QTest::addColumn<QList<int>>("apkFileCounts");
    QTest::newRow("teapot") << "teapot" << QStringList("com.sample.teapot")
                            << (QList<int>() << (13 + 3*archCount));
    QTest::newRow("no native") << "no-native"
            << QStringList("com.example.android.basicmediadecoder") << (QList<int>() << 22);
    QTest::newRow("multiple libs") << "multiple-libs-per-apk" << QStringList("twolibs")
                                   << (QList<int>() << (6 + 4 * archCount));
    QTest::newRow("multiple apks") << "multiple-apks-per-project"
                                   << (QStringList() << "twolibs1" << "twolibs2")
                                   << QList<int>({6 + archCount * 3 + 2, 5 + archCount * 4});
}

QTEST_MAIN(TestBlackboxAndroid)
