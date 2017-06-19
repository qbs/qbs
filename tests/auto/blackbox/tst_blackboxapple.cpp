/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "tst_blackboxapple.h"

#include "../shared.h"
#include <tools/hostosinfo.h>
#include <tools/profile.h>

#include <QtCore/qjsondocument.h>
#include <QtXml/qdom.h>

#include <regex>

#define WAIT_FOR_NEW_TIMESTAMP() waitForNewTimestamp(testDataDir)

using qbs::Internal::HostOsInfo;
using qbs::Profile;

class QFileInfo2 : public QFileInfo {
public:
    QFileInfo2(const QString &path) : QFileInfo(path) { }
    bool isRegularFile() const { return isFile() && !isSymLink(); }
    bool isRegularDir() const { return isDir() && !isSymLink(); }
    bool isFileSymLink() const { return isFile() && isSymLink(); }
    bool isDirSymLink() const { return isDir() && isSymLink(); }
};

static QString getEmbeddedBinaryPlist(const QString &file)
{
    QProcess p;
    p.start("otool", QStringList() << "-v" << "-X" << "-s" << "__TEXT" << "__info_plist" << file);
    p.waitForFinished();
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

TestBlackboxApple::TestBlackboxApple()
    : TestBlackboxBase (SRCDIR "/testdata-apple", "blackbox-apple")
{
}

void TestBlackboxApple::initTestCase()
{
    if (!HostOsInfo::isMacosHost()) {
        QSKIP("only applies on macOS");
        return;
    }

    TestBlackboxBase::initTestCase();
}

void TestBlackboxApple::appleMultiConfig()
{
    QDir::setCurrent(testDataDir + "/apple-multiconfig");
    QCOMPARE(runQbs(), 0);

    QVERIFY(QFileInfo2(defaultInstallRoot + "/singleapp.app/Contents/MacOS/singleapp").isExecutable());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/singleapp.app/Contents/Info.plist").isRegularFile());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/singleapp.app/Contents/PkgInfo").isRegularFile());

    QVERIFY(QFileInfo2(defaultInstallRoot + "/singleapp_agg.app/Contents/MacOS/singleapp_agg").isExecutable());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/singleapp_agg.app/Contents/Info.plist").isRegularFile());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/singleapp_agg.app/Contents/PkgInfo").isRegularFile());

    QVERIFY(QFileInfo2(defaultInstallRoot + "/singlelib.framework/singlelib").isFileSymLink());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/singlelib.framework/Resources").isDirSymLink());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/singlelib.framework/Versions").isRegularDir());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/singlelib.framework/Versions/A").isRegularDir());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/singlelib.framework/Versions/A/singlelib").isRegularFile());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/singlelib.framework/Versions/A/Resources").isRegularDir());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/singlelib.framework/Versions/A/Resources/Info.plist").isRegularFile());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/singlelib.framework/Versions/Current").isDirSymLink());

    QVERIFY(QFileInfo2(defaultInstallRoot + "/multiapp.app/Contents/MacOS/multiapp").isExecutable());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/multiapp.app/Contents/Info.plist").isRegularFile());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/multiapp.app/Contents/PkgInfo").isRegularFile());

    QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiapp.app/Contents/MacOS/fatmultiapp").isFileSymLink());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiapp.app/Contents/MacOS/fatmultiapp_debug").isExecutable());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiapp.app/Contents/Info.plist").isRegularFile());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiapp.app/Contents/PkgInfo").isRegularFile());

    QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/multilib").isFileSymLink());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Resources").isDirSymLink());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Versions").isRegularDir());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Versions/A").isRegularDir());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Versions/A/multilib").isRegularFile());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Versions/A/multilib_debug").isRegularFile());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Versions/A/multilib_profile").isRegularFile());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Versions/A/Resources").isRegularDir());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Versions/A/Resources/Info.plist").isRegularFile());
    QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Versions/Current").isDirSymLink());

    for (const QString &variant : { "release", "debug", "profile" }) {
        for (const QString &arch : { "x86_64" }) {
            QProcess process;
            process.setProgram("/usr/bin/arch");
            process.setArguments({
                "-arch", arch,
                "-e", "DYLD_IMAGE_SUFFIX=_" + variant,
                defaultInstallRoot + "/multiapp.app/Contents/MacOS/multiapp"
            });
            process.start();
            process.waitForFinished();
            QCOMPARE(process.exitCode(), 0);
            const auto processStdout = process.readAllStandardOutput();
            QVERIFY2(processStdout.contains("Hello from " + variant.toUtf8() + " " + arch.toUtf8()),
                     processStdout.constData());
        }
    }
}

void TestBlackboxApple::assetCatalog()
{
    QFETCH(bool, flatten);

    const auto xcodeVersion = findXcodeVersion();

    QDir::setCurrent(testDataDir + QLatin1String("/ib/assetcatalog"));

    rmDirR(relativeBuildDir());

    QbsRunParameters params;
    const QString flattens = "modules.ib.flatten:" + QString(flatten ? "true" : "false");

    // Make sure a dry run does not write anything
    params.arguments = QStringList() << "-f" << "assetcatalogempty.qbs" << "--dry-run" << flattens;
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!directoryExists(relativeBuildDir()));

    params.arguments = QStringList() << "-f" << "assetcatalogempty.qbs" << flattens;
    QCOMPARE(runQbs(params), 0);

    // empty asset catalogs must still produce output
    if (xcodeVersion >= qbs::Internal::Version(5))
        QVERIFY((bool)m_qbsStdout.contains("compiling empty.xcassets"));

    // should not produce a CAR since minimumMacosVersion will be < 10.9
    QVERIFY(!regularFileExists(relativeProductBuildDir("assetcatalogempty") + "/assetcatalogempty.app/Contents/Resources/Assets.car"));

    rmDirR(relativeBuildDir());
    params.arguments.append("modules.cpp.minimumMacosVersion:10.9"); // force CAR generation
    QCOMPARE(runQbs(params), 0);

    // empty asset catalogs must still produce output
    if (xcodeVersion >= qbs::Internal::Version(5)) {
        QVERIFY((bool)m_qbsStdout.contains("compiling empty.xcassets"));
        // No matter what, we need a 10.9 host to build CAR files
        if (HostOsInfo::hostOsVersion() >= qbs::Internal::Version(10, 9)) {
            QVERIFY(regularFileExists(relativeProductBuildDir("assetcatalogempty")
                                      + "/assetcatalogempty.app/Contents/Resources/Assets.car"));
        } else {
            QVERIFY(regularFileExists(relativeProductBuildDir("assetcatalogempty")
                                      + "/assetcatalogempty.app/Contents/Resources/empty.icns"));
            QVERIFY(regularFileExists(relativeProductBuildDir("assetcatalogempty")
                                      + "/assetcatalogempty.app/Contents/Resources/other.png"));
            QVERIFY(regularFileExists(relativeProductBuildDir("assetcatalogempty")
                                      + "/assetcatalogempty.app/Contents/Resources/other@2x.png"));
        }
    }

    // this asset catalog happens to have an embedded icon set,
    // but this should NOT be built since it is not in the files list
    QVERIFY(!(bool)m_qbsStdout.contains(".iconset"));

    // now we'll add the iconset
    rmDirR(relativeBuildDir());
    params.arguments.append("project.includeIconset:true");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!(bool)m_qbsStdout.contains("compiling empty.xcassets"));
    QVERIFY((bool)m_qbsStdout.contains("compiling empty.iconset"));

    // make sure the nibs/storyboards are in there
    QString nib = relativeProductBuildDir("assetcatalogempty") + "/assetcatalogempty.app/Contents/Resources/MainMenu.nib";
    QStringList nibFiles;
    if (flatten) {
        QVERIFY(regularFileExists(nib));
    } else {
        QVERIFY(directoryExists(nib));
        nibFiles = QStringList() << "designable.nib" << "keyedobjects.nib";
    }

    QString storyboardc = relativeProductBuildDir("assetcatalogempty") + "/assetcatalogempty.app/Contents/Resources/Storyboard.storyboardc";
    QStringList storyboardcFiles;
    if (HostOsInfo::hostOsVersion() >= qbs::Internal::Version(10, 10)) {
        QVERIFY(directoryExists(storyboardc));

        storyboardcFiles = QStringList()
                << "1os-k8-h10-view-qKA-a5-eUe.nib"
                << "Info.plist"
                << "Iqk-Fi-Vhk-view-HRv-3O-Qxh.nib"
                << "Main.nib"
                << "NSViewController-Iqk-Fi-Vhk.nib"
                << "NSViewController-Yem-rc-72E.nib"
                << "Yem-rc-72E-view-ODp-aO-Dmf.nib";

        if (!flatten) {
            storyboardcFiles << "designable.storyboard";
            storyboardcFiles.sort();
        }
    }

    QCOMPARE(QDir(nib).entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name), nibFiles);
    QCOMPARE(QDir(storyboardc).entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name), storyboardcFiles);
    QbsRunParameters params2 = params;
    params2.command = "clean";
    QCOMPARE(runQbs(params2), 0);
    QCOMPARE(QDir(nib).entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name), QStringList());
    QCOMPARE(QDir(storyboardc).entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name), QStringList());
}

void TestBlackboxApple::assetCatalog_data()
{
    QTest::addColumn<bool>("flatten");
    QTest::newRow("flattened") << true;
    QTest::newRow("unflattened") << false;
}

void TestBlackboxApple::assetCatalogsEmpty() {
    if (findXcodeVersion() < qbs::Internal::Version(5))
        QSKIP("requires Xcode 5 or above");
    QDir::setCurrent(testDataDir + QLatin1String("/ib/empty-asset-catalogs"));
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling assetcatalog1.xcassets"), m_qbsStdout);
    QVERIFY2(!m_qbsStdout.contains("compiling assetcatalog2.xcassets"), m_qbsStdout);
}

void TestBlackboxApple::assetCatalogsMultiple() {
    if (findXcodeVersion() < qbs::Internal::Version(5))
        QSKIP("requires Xcode 5 or above");
    QDir::setCurrent(testDataDir + QLatin1String("/ib/multiple-asset-catalogs"));
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("compiling assetcatalog1.xcassets"), m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains("compiling assetcatalog2.xcassets"), m_qbsStdout);
}

void TestBlackboxApple::bundleStructure()
{
    QFETCH(QString, productName);
    QFETCH(QString, productTypeIdentifier);
    QFETCH(bool, isShallow);

    QDir::setCurrent(testDataDir + "/bundle-structure");
    QbsRunParameters params;
    params.arguments << "project.buildableProducts:" + productName;
    if (isShallow) {
        // Coerce shallow bundles - don't set bundle.isShallow directly because we want to test the
        // automatic detection
        const auto xcode5 = findXcodeVersion() >= qbs::Internal::Version(5);
        params.arguments
                << "qbs.targetOS:ios,darwin,bsd,unix"
                << (xcode5 ? "qbs.architectures:arm64" : "qbs.architectures:armv7a");
    }

    if (productName == "ABadApple" || productName == "ABadThirdParty")
        params.expectFailure = true;

    rmDirR(relativeBuildDir());
    const int status = runQbs(params);
    if (status != 0) {
        QVERIFY2(m_qbsStderr.contains("Bundle product type "
                                      + productTypeIdentifier.toLatin1()
                                      + " is not supported."),
                 m_qbsStderr.constData());
        return;
    }

    QCOMPARE(status, 0);

    if (!isShallow) {
        if (productName == "A") {
            QVERIFY(QFileInfo2(defaultInstallRoot + "/A.app").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/A.app/Contents").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/A.app/Contents/Info.plist").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/A.app/Contents/MacOS").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/A.app/Contents/MacOS/A").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/A.app/Contents/PkgInfo").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/A.app/Contents/Resources").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/A.app/Contents/Resources/resource.txt").isRegularFile());
        }

        if (productName == "B") {
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/B").isFileSymLink());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Headers").isDirSymLink());
            //QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Modules").isDirSymLink());
            QVERIFY(!QFileInfo2(defaultInstallRoot + "/B.framework/PkgInfo").exists());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/PrivateHeaders").isDirSymLink());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Resources").isDirSymLink());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Versions").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Versions/A").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Versions/A/B").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Versions/A/Headers").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Versions/A/Headers/dummy.h").isRegularFile());
            //QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Versions/A/Modules").isRegularDir());
            //QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Versions/A/Modules/module.modulemap").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Versions/A/PrivateHeaders").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Versions/A/PrivateHeaders/dummy_p.h").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Versions/A/Resources").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Versions/A/Resources/Info.plist").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Versions/Current").isDirSymLink());
        }

        if (productName == "C") {
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/C").isFileSymLink());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Headers").isDirSymLink());
            //QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Modules").isDirSymLink());
            QVERIFY(!QFileInfo2(defaultInstallRoot + "/C.framework/PkgInfo").exists());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/PrivateHeaders").isDirSymLink());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Resources").isDirSymLink());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Versions").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Versions/A").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Versions/A/C").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Versions/A/Headers").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Versions/A/Headers/dummy.h").isRegularFile());
            //QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Versions/A/Modules").isRegularDir());
            //QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Versions/A/Modules/module.modulemap").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Versions/A/PrivateHeaders").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Versions/A/PrivateHeaders/dummy_p.h").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Versions/A/Resources").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Versions/A/Resources/Info.plist").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Versions/Current").isDirSymLink());
        }

        if (productName == "D") {
            QVERIFY(QFileInfo2(defaultInstallRoot + "/D.bundle").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/D.bundle/Contents").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/D.bundle/Contents/Info.plist").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/D.bundle/Contents/MacOS").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/D.bundle/Contents/MacOS/D").isRegularFile());
            QVERIFY(!QFileInfo2(defaultInstallRoot + "/D.bundle/Contents/PkgInfo").exists());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/D.bundle/Contents/Resources").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/D.bundle/Contents/Resources/resource.txt").isRegularFile());
        }

        if (productName == "E") {
            QVERIFY(QFileInfo2(defaultInstallRoot + "/E.appex").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/E.appex/Contents").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/E.appex/Contents/Info.plist").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/E.appex/Contents/MacOS").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/E.appex/Contents/MacOS/E").isRegularFile());
            QVERIFY(!QFileInfo2(defaultInstallRoot + "/E.appex/Contents/PkgInfo").exists());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/E.appex/Contents/Resources").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/E.appex/Contents/Resources/resource.txt").isRegularFile());
        }

        if (productName == "F") {
            QVERIFY(QFileInfo2(defaultInstallRoot + "/F.xpc").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/F.xpc/Contents").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/F.xpc/Contents/Info.plist").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/F.xpc/Contents/MacOS").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/F.xpc/Contents/MacOS/F").isRegularFile());
            QVERIFY(!QFileInfo2(defaultInstallRoot + "/F.xpc/Contents/PkgInfo").exists());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/F.xpc/Contents/Resources").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/F.xpc/Contents/Resources/resource.txt").isRegularFile());
        }

        if (productName == "G") {
            QVERIFY(QFileInfo2(defaultInstallRoot + "/G").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/G/ContentInfo.plist").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/G/Contents/resource.txt").isRegularFile());
        }
    } else {
        if (productName == "A") {
            QVERIFY(QFileInfo2(defaultInstallRoot + "/A.app").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/A.app/A").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/A.app/Info.plist").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/A.app/PkgInfo").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/A.app/resource.txt").isRegularFile());
        }

        if (productName == "B") {
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/B").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Headers").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Headers/dummy.h").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Info.plist").isRegularFile());
            //QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Modules").isRegularDir());
            //QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/Modules/module.modulemap").isRegularFile());
            QVERIFY(!QFileInfo2(defaultInstallRoot + "/B.framework/PkgInfo").exists());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/PrivateHeaders").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/PrivateHeaders/dummy_p.h").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/resource.txt").isRegularFile());
        }

        if (productName == "C") {
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/C").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Headers").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Headers/dummy.h").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Info.plist").isRegularFile());
            //QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Modules").isRegularDir());
            //QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/Modules/module.modulemap").isRegularFile());
            QVERIFY(!QFileInfo2(defaultInstallRoot + "/C.framework/PkgInfo").exists());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/PrivateHeaders").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/PrivateHeaders/dummy_p.h").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/resource.txt").isRegularFile());
        }

        if (productName == "D") {
            QVERIFY(QFileInfo2(defaultInstallRoot + "/D.bundle").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/D.bundle/D").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/D.bundle/Headers").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/D.bundle/Headers/dummy.h").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/D.bundle/Info.plist").isRegularFile());
            QVERIFY(!QFileInfo2(defaultInstallRoot + "/D.bundle/PkgInfo").exists());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/D.bundle/PrivateHeaders").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/D.bundle/PrivateHeaders/dummy_p.h").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/D.bundle/resource.txt").isRegularFile());
        }

        if (productName == "E") {
            QVERIFY(QFileInfo2(defaultInstallRoot + "/E.appex").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/E.appex/E").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/E.appex/Headers").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/E.appex/Headers/dummy.h").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/E.appex/Info.plist").isRegularFile());
            QVERIFY(!QFileInfo2(defaultInstallRoot + "/E.appex/PkgInfo").exists());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/E.appex/PrivateHeaders").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/E.appex/PrivateHeaders/dummy_p.h").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/E.appex/resource.txt").isRegularFile());
        }

        if (productName == "F") {
            QVERIFY(QFileInfo2(defaultInstallRoot + "/F.xpc").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/F.xpc/F").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/F.xpc/Headers").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/F.xpc/Headers/dummy.h").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/F.xpc/Info.plist").isRegularFile());
            QVERIFY(!QFileInfo2(defaultInstallRoot + "/F.xpc/PkgInfo").exists());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/F.xpc/PrivateHeaders").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/F.xpc/PrivateHeaders/dummy_p.h").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/F.xpc/resource.txt").isRegularFile());
        }

        if (productName == "G") {
            QVERIFY(QFileInfo2(defaultInstallRoot + "/G").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/G/ContentInfo.plist").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/G/Contents/resource.txt").isRegularFile());
        }
    }
}

void TestBlackboxApple::bundleStructure_data()
{
    QTest::addColumn<QString>("productName");
    QTest::addColumn<QString>("productTypeIdentifier");
    QTest::addColumn<bool>("isShallow");

    const auto addRows = [](bool isShallow) {
        const QString s = (isShallow ? " shallow" : "");
        QTest::newRow(("A" + s).toLatin1()) << "A" << "com.apple.product-type.application" << isShallow;
        QTest::newRow(("ABadApple" + s).toLatin1()) << "ABadApple" << "com.apple.product-type.will.never.exist.ever.guaranteed" << isShallow;
        QTest::newRow(("ABadThirdParty" + s).toLatin1()) << "ABadThirdParty" << "org.special.third.party.non.existent.product.type" << isShallow;
        QTest::newRow(("B" + s).toLatin1()) << "B" << "com.apple.product-type.framework" << isShallow;
        QTest::newRow(("C" + s).toLatin1()) << "C" << "com.apple.product-type.framework.static" << isShallow;
        QTest::newRow(("D" + s).toLatin1()) << "D" << "com.apple.product-type.bundle" << isShallow;
        QTest::newRow(("E" + s).toLatin1()) << "E" << "com.apple.product-type.app-extension" << isShallow;
        QTest::newRow(("F" + s).toLatin1()) << "F" << "com.apple.product-type.xpc-service" << isShallow;
        QTest::newRow(("G" + s).toLatin1()) << "G" << "com.apple.product-type.in-app-purchase-content" << isShallow;
    };

    addRows(true);
    addRows(false);
}

void TestBlackboxApple::deploymentTarget()
{
    QFETCH(QString, sdk);
    QFETCH(QString, os);
    QFETCH(QString, arch);
    QFETCH(QString, cflags);
    QFETCH(QString, lflags);

    QDir::setCurrent(testDataDir + "/deploymentTarget");

    QbsRunParameters params;
    params.arguments = QStringList()
            << "--command-echo-mode"
            << "command-line"
            << "qbs.targetOS:" + os
            << "qbs.architectures:" + arch;

    rmDirR(relativeBuildDir());
    int status = runQbs(params);

    const QStringList skippableMessages = QStringList()
            << "There is no matching SDK available for " + sdk + "."
            << "x86_64h will be mis-detected as x86_64 with Apple Clang < 6.0"
            << "clang: error: unknown argument: '-mtvos-version-min"
            << "clang: error: unknown argument: '-mtvos-simulator-version-min"
            << "clang: error: unknown argument: '-mwatchos-version-min"
            << "clang: error: unknown argument: '-mwatchos-simulator-version-min";
    if (status != 0) {
        for (const auto &message : skippableMessages) {
            if (m_qbsStderr.contains(message.toUtf8()))
                QSKIP(message.toUtf8());
        }
    }

    QCOMPARE(status, 0);
    QVERIFY2(m_qbsStderr.contains(cflags.toLatin1()), m_qbsStderr.constData());
    QVERIFY2(m_qbsStderr.contains(lflags.toLatin1()), m_qbsStderr.constData());
}

void TestBlackboxApple::deploymentTarget_data()
{
    static const QString macos = QStringLiteral("macos,darwin,bsd,unix");
    static const QString ios = QStringLiteral("ios,darwin,bsd,unix");
    static const QString ios_sim = QStringLiteral("ios-simulator,") + ios;
    static const QString tvos = QStringLiteral("tvos,darwin,bsd,unix");
    static const QString tvos_sim = QStringLiteral("tvos-simulator,") + tvos;
    static const QString watchos = QStringLiteral("watchos,darwin,bsd,unix");
    static const QString watchos_sim = QStringLiteral("watchos-simulator,") + watchos;

    QTest::addColumn<QString>("sdk");
    QTest::addColumn<QString>("os");
    QTest::addColumn<QString>("arch");
    QTest::addColumn<QString>("cflags");
    QTest::addColumn<QString>("lflags");

    QTest::newRow("macos x86") << "macosx" << macos << "x86"
                         << "-triple i386-apple-macosx10.4"
                         << "-macosx_version_min 10.4";
    QTest::newRow("macos x86_64") << "macosx" << macos << "x86_64"
                         << "-triple x86_64-apple-macosx10.4"
                         << "-macosx_version_min 10.4";

    const auto xcodeVersion = findXcodeVersion();
    if (xcodeVersion >= qbs::Internal::Version(6))
        QTest::newRow("macos x86_64h") << "macosx" << macos << "x86_64h"
                             << "-triple x86_64h-apple-macosx10.12"
                             << "-macosx_version_min 10.12";

    QTest::newRow("ios armv7a") << "iphoneos" << ios << "armv7a"
                         << "-triple thumbv7-apple-ios6.0"
                         << "-iphoneos_version_min 6.0";
    QTest::newRow("ios armv7s") << "iphoneos" <<ios << "armv7s"
                         << "-triple thumbv7s-apple-ios7.0"
                         << "-iphoneos_version_min 7.0";
    if (xcodeVersion >= qbs::Internal::Version(5))
        QTest::newRow("ios arm64") << "iphoneos" <<ios << "arm64"
                             << "-triple arm64-apple-ios7.0"
                             << "-iphoneos_version_min 7.0";
    QTest::newRow("ios-simulator x86") << "iphonesimulator" << ios_sim << "x86"
                             << "-triple i386-apple-ios6.0"
                             << "-ios_simulator_version_min 6.0";
    if (xcodeVersion >= qbs::Internal::Version(5))
        QTest::newRow("ios-simulator x86_64") << "iphonesimulator" << ios_sim << "x86_64"
                                 << "-triple x86_64-apple-ios7.0"
                                 << "-ios_simulator_version_min 7.0";

    if (xcodeVersion >= qbs::Internal::Version(7)) {
        if (xcodeVersion >= qbs::Internal::Version(7, 1)) {
            QTest::newRow("tvos arm64") << "appletvos" << tvos << "arm64"
                                 << "-triple arm64-apple-tvos9.0"
                                 << "-tvos_version_min 9.0";
            QTest::newRow("tvos-simulator x86_64") << "appletvsimulator" << tvos_sim << "x86_64"
                                     << "-triple x86_64-apple-tvos9.0"
                                     << "-tvos_simulator_version_min 9.0";
        }

        QTest::newRow("watchos armv7k") << "watchos" << watchos << "armv7k"
                             << "-triple thumbv7k-apple-watchos2.0"
                             << "-watchos_version_min 2.0";
        QTest::newRow("watchos-simulator x86") << "watchsimulator" << watchos_sim << "x86"
                                 << "-triple i386-apple-watchos2.0"
                                 << "-watchos_simulator_version_min 2.0";
    }
}

void TestBlackboxApple::dmg()
{
    QDir::setCurrent(testDataDir + "/apple-dmg");
    QCOMPARE(runQbs(), 0);
}

void TestBlackboxApple::embedInfoPlist()
{
    QDir::setCurrent(testDataDir + QLatin1String("/embedInfoPlist"));

    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);

    QVERIFY(!getEmbeddedBinaryPlist(defaultInstallRoot + "/app").isEmpty());
    QVERIFY(!getEmbeddedBinaryPlist(defaultInstallRoot + "/liblib.dylib").isEmpty());
    QVERIFY(!getEmbeddedBinaryPlist(defaultInstallRoot + "/mod.bundle").isEmpty());

    params.arguments = QStringList(QLatin1String("modules.bundle.embedInfoPlist:false"));
    params.expectFailure = true;
    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(params), 0);

    QVERIFY(getEmbeddedBinaryPlist(defaultInstallRoot + "/app").isEmpty());
    QVERIFY(getEmbeddedBinaryPlist(defaultInstallRoot + "/liblib.dylib").isEmpty());
    QVERIFY(getEmbeddedBinaryPlist(defaultInstallRoot + "/mod.bundle").isEmpty());
}

void TestBlackboxApple::frameworkStructure()
{
    QDir::setCurrent(testDataDir + QLatin1String("/frameworkStructure"));

    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);

    QVERIFY(regularFileExists(relativeProductBuildDir("Widget") + "/Widget.framework/Versions/A/Widget"));
    QVERIFY(regularFileExists(relativeProductBuildDir("Widget") + "/Widget.framework/Versions/A/Headers/Widget.h"));
    QVERIFY(regularFileExists(relativeProductBuildDir("Widget") + "/Widget.framework/Versions/A/PrivateHeaders/WidgetPrivate.h"));
    QVERIFY(regularFileExists(relativeProductBuildDir("Widget") + "/Widget.framework/Versions/A/Resources/BaseResource"));
    QVERIFY(regularFileExists(relativeProductBuildDir("Widget") + "/Widget.framework/Versions/A/Resources/en.lproj/EnglishResource"));
    QVERIFY(directoryExists(relativeProductBuildDir("Widget") + "/Widget.framework/Versions/Current"));
    QVERIFY(regularFileExists(relativeProductBuildDir("Widget") + "/Widget.framework/Widget"));
    QVERIFY(directoryExists(relativeProductBuildDir("Widget") + "/Widget.framework/Headers"));
    QVERIFY(directoryExists(relativeProductBuildDir("Widget") + "/Widget.framework/PrivateHeaders"));
    QVERIFY(directoryExists(relativeProductBuildDir("Widget") + "/Widget.framework/Resources"));

    params.command = "resolve";
    params.arguments = QStringList() << "project.includeHeaders:false";
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(), 0);

    QVERIFY(!directoryExists(relativeProductBuildDir("Widget") + "/Widget.framework/Headers"));
    QVERIFY(!directoryExists(relativeProductBuildDir("Widget") + "/Widget.framework/PrivateHeaders"));
}

void TestBlackboxApple::iconset()
{
    QDir::setCurrent(testDataDir + QLatin1String("/ib/iconset"));

    QbsRunParameters params;
    params.arguments = QStringList() << "-f" << "iconset.qbs";
    QCOMPARE(runQbs(params), 0);

    QVERIFY(regularFileExists(relativeProductBuildDir("iconset") + "/white.icns"));
}

void TestBlackboxApple::iconsetApp()
{
    QDir::setCurrent(testDataDir + QLatin1String("/ib/iconsetapp"));

    QbsRunParameters params;
    params.arguments = QStringList() << "-f" << "iconsetapp.qbs";
    QCOMPARE(runQbs(params), 0);

    QVERIFY(regularFileExists(relativeProductBuildDir("iconsetapp") + "/iconsetapp.app/Contents/Resources/white.icns"));
}

void TestBlackboxApple::infoPlist()
{
    QDir::setCurrent(testDataDir + "/infoplist");

    QbsRunParameters params;
    params.arguments = QStringList() << "-f" << "infoplist.qbs";
    QCOMPARE(runQbs(params), 0);

    QFile infoplist(relativeProductBuildDir("infoplist") + "/infoplist.app/Contents/Info.plist");
    QVERIFY(infoplist.open(QIODevice::ReadOnly));
    const QByteArray fileContents = infoplist.readAll();
    QVERIFY2(fileContents.contains("<key>LSMinimumSystemVersion</key>"), fileContents.constData());
    QVERIFY2(fileContents.contains("<string>10.7</string>"), fileContents.constData());
    QVERIFY2(fileContents.contains("<key>NSPrincipalClass</key>"), fileContents.constData());
}

void TestBlackboxApple::objcArc()
{
    QDir::setCurrent(testDataDir + QLatin1String("/objc-arc"));

    QCOMPARE(runQbs(), 0);
}

void TestBlackboxApple::xcode()
{
    QProcess xcodeSelect;
    xcodeSelect.start("xcode-select", QStringList() << "--print-path");
    QVERIFY2(xcodeSelect.waitForStarted(), qPrintable(xcodeSelect.errorString()));
    QVERIFY2(xcodeSelect.waitForFinished(), qPrintable(xcodeSelect.errorString()));
    QVERIFY2(xcodeSelect.exitCode() == 0, qPrintable(xcodeSelect.readAllStandardError().constData()));
    const QString developerPath(QString::fromLocal8Bit(xcodeSelect.readAllStandardOutput().trimmed()));

    std::multimap<std::string, std::string> sdks;

    QProcess xcodebuildShowSdks;
    xcodebuildShowSdks.start("xcrun", QStringList() << "xcodebuild" << "-showsdks");
    QVERIFY2(xcodebuildShowSdks.waitForStarted(), qPrintable(xcodebuildShowSdks.errorString()));
    QVERIFY2(xcodebuildShowSdks.waitForFinished(), qPrintable(xcodebuildShowSdks.errorString()));
    QVERIFY2(xcodebuildShowSdks.exitCode() == 0, qPrintable(xcodebuildShowSdks.readAllStandardError().constData()));
    for (const QString &line : QString::fromLocal8Bit(xcodebuildShowSdks.readAllStandardOutput().trimmed()).split('\n', QString::SkipEmptyParts)) {
        static const std::regex regexp("^.+\\s+\\-sdk\\s+([a-z]+)([0-9]+\\.[0-9]+)$");
        const auto ln = line.toStdString();
        std::smatch match;
        if (std::regex_match(ln, match, regexp))
            sdks.insert({ match[1], match[2] });
    }

    auto range = sdks.equal_range("macosx");
    QStringList sdkValues;
    for (auto i = range.first; i != range.second; ++i)
        sdkValues.push_back(QString::fromStdString(i->second));

    QDir::setCurrent(testDataDir + "/xcode");
    QbsRunParameters params;
    params.arguments = (QStringList()
                        << (QStringLiteral("modules.xcode.developerPath:") + developerPath)
                        << (QStringLiteral("project.sdks:['") + sdkValues.join("','") + "']"));
    QCOMPARE(runQbs(params), 0);
}

QTEST_MAIN(TestBlackboxApple)

QVariantMap TestBlackboxApple::findXcode(int *status)
{
    QTemporaryDir temp;
    QbsRunParameters params = QStringList({"-f", testDataDir + "/find/find-xcode.qbs"});
    params.profile = "none";
    params.buildDirectory = temp.path();
    const int res = runQbs(params);
    if (status)
        *status = res;
    QFile file(temp.path() + "/" + relativeProductBuildDir("find-xcode")
               + "/xcode.json");
    if (!file.open(QIODevice::ReadOnly))
        return QVariantMap { };
    return QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
}

qbs::Internal::Version TestBlackboxApple::findXcodeVersion()
{
    return qbs::Internal::Version::fromString(findXcode().value("version").toString());
}
