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
#include <tools/qttools.h>

#include <QtCore/qdiriterator.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
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

static QVariantMap readInfoPlistFile(const QString &infoPlistPath)
{
    if (!QFile::exists(infoPlistPath)) {
        qWarning() << infoPlistPath << "doesn't exist";
        return {};
    }

    QProcess plutil;
    plutil.start("plutil", {
                     QStringLiteral("-convert"),
                     QStringLiteral("json"),
                     infoPlistPath
                 });
    if (!plutil.waitForStarted()) {
        qWarning() << plutil.errorString();
        return {};
    }
    if (!plutil.waitForFinished()) {
        qWarning() << plutil.errorString();
        return {};
    }
    if (plutil.exitCode() != 0) {
        qWarning() << plutil.readAllStandardError().constData();
        return {};
    }

    QFile infoPlist(infoPlistPath);
    if (!infoPlist.open(QIODevice::ReadOnly)) {
        qWarning() << infoPlist.errorString();
        return {};
    }
    QJsonParseError error;
    const auto json = QJsonDocument::fromJson(infoPlist.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << error.errorString();
        return {};
    }
    return json.object().toVariantMap();
}

static QString getInfoPlistPath(const QString &bundlePath)
{
    QFileInfo contents(bundlePath + "/Contents");
    if (contents.exists() && contents.isDir())
        return contents.filePath() + "/Info.plist"; // macOS bundle
    return bundlePath + "/Info.plist";
}

static bool testVariantListType(const QVariant &variant, QMetaType::Type type)
{
    if (variant.userType() != QMetaType::QVariantList)
        return false;
    for (const auto &value : variant.toList()) {
        if (value.userType() != type)
            return false;
    }
    return true;
}

static QString findFatLibrary(const QString &dir, const QString &libraryName)
{
    QDirIterator it(dir, {}, QDir::AllEntries, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (it.fileInfo().fileName() == libraryName) {
            QProcess lipo;
            lipo.start("lipo", { QStringLiteral("-info"), it.filePath() });
            if (!lipo.waitForStarted() || !lipo.waitForFinished() || lipo.exitCode() != 0)
                return {};
            auto output = lipo.readAllStandardOutput();
            if (output.contains(QByteArrayLiteral("Architectures in the fat file")))
                return QDir::cleanPath(it.filePath());
        }
    }

    return {};
}

enum class CodeSignResult { Failed = 0, Signed, Unsigned };
using CodeSignData = QMap<QByteArray, QByteArray>;
static std::pair<CodeSignResult, CodeSignData> parseCodeSignOutput(const QByteArray &output)
{
    CodeSignData data;
    if (output.contains("code object is not signed at all"))
        return {CodeSignResult::Unsigned, data};
    const auto lines = output.split('\n');
    for (const auto &line: lines) {
        if (line.isEmpty()
                || line.startsWith("CodeDirectory")
                || line.startsWith("Sealed Resources")
                || line.startsWith("Internal requirements")) {
            continue;
        }
        const int index = line.indexOf('=');
        if (index == -1)
            return {CodeSignResult::Failed, {}};
        data[line.mid(0, index)] = line.mid(index + 1);
    }
    return {CodeSignResult::Signed, data};
}

static std::pair<CodeSignResult, CodeSignData> getCodeSignInfo(const QString &path)
{
    QProcess codesign;
    codesign.start("codesign", { QStringLiteral("-dv"), path });
    if (!codesign.waitForStarted() || !codesign.waitForFinished())
        return {CodeSignResult::Failed, {}};
    const auto output = codesign.readAllStandardError();
    return parseCodeSignOutput(output);
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
    const auto xcodeVersion = findXcodeVersion();
    if (!xcodeVersion)
        QSKIP("requires Xcode profile");
    QDir::setCurrent(testDataDir + "/apple-multiconfig");
    QCOMPARE(runQbs(QbsRunParameters(QStringList{
        "qbs.installPrefix:''",
        QStringLiteral("project.xcodeVersion:") + xcodeVersion->toString()})), 0);

    if (m_qbsStdout.contains("isShallow: false")) {
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

        QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiapp.app/Contents/MacOS/fatmultiapp").isExecutable());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiapp.app/Contents/Info.plist").isRegularFile());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiapp.app/Contents/PkgInfo").isRegularFile());

        QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiappmultivariant.app/Contents/MacOS/"
                                                "fatmultiappmultivariant").isFileSymLink());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiappmultivariant.app/Contents/MacOS/"
                                                "fatmultiappmultivariant_debug").isExecutable());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiappmultivariant.app/Contents/MacOS/"
                                                "fatmultiappmultivariant_profiling").isExecutable());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiappmultivariant.app/Contents/Info.plist")
                .isRegularFile());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiappmultivariant.app/Contents/PkgInfo")
                .isRegularFile());

        QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/multilib").isFileSymLink());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Resources").isDirSymLink());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Versions").isRegularDir());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Versions/A").isRegularDir());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Versions/A/multilib").isRegularFile());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Versions/A/multilib_debug").isRegularFile());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Versions/A/multilib_profiling").isRegularFile());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Versions/A/Resources").isRegularDir());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Versions/A/Resources/Info.plist").isRegularFile());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Versions/Current").isDirSymLink());

        for (const QString variant : { "release", "debug", "profiling" }) {
            for (const QString arch : { "x86_64" }) {
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
    } else if (m_qbsStdout.contains("isShallow: true")) {
        QVERIFY(QFileInfo2(defaultInstallRoot + "/singleapp.app/singleapp").isExecutable());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/singleapp.app/Info.plist").isRegularFile());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/singleapp.app/PkgInfo").isRegularFile());

        QVERIFY(QFileInfo2(defaultInstallRoot + "/singleapp_agg.app/singleapp_agg").isExecutable());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/singleapp_agg.app/Info.plist").isRegularFile());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/singleapp_agg.app/PkgInfo").isRegularFile());

        QVERIFY(QFileInfo2(defaultInstallRoot + "/singlelib.framework/singlelib").isExecutable());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/singlelib.framework/Info.plist").isRegularFile());

        QVERIFY(QFileInfo2(defaultInstallRoot + "/multiapp.app/multiapp").isExecutable());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/multiapp.app/Info.plist").isRegularFile());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/multiapp.app/PkgInfo").isRegularFile());

        QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiapp.app/fatmultiapp").isExecutable());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiapp.app/Info.plist").isRegularFile());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiapp.app/PkgInfo").isRegularFile());

        QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiappmultivariant.app/"
                                                "fatmultiappmultivariant").isFileSymLink());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiappmultivariant.app/"
                                                "fatmultiappmultivariant_debug").isExecutable());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiappmultivariant.app/"
                                                "fatmultiappmultivariant_profiling").isExecutable());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiappmultivariant.app/Info.plist")
                .isRegularFile());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/fatmultiappmultivariant.app/PkgInfo")
                .isRegularFile());

        QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/multilib").isRegularFile());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/multilib_debug").isRegularFile());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/multilib_profiling").isRegularFile());
        QVERIFY(QFileInfo2(defaultInstallRoot + "/multilib.framework/Info.plist").isRegularFile());
    } else {
        QVERIFY2(false, qPrintable(m_qbsStdout));
    }
}

void TestBlackboxApple::aggregateDependencyLinking()
{
    const auto xcodeVersion = findXcodeVersion();

    if (!xcodeVersion)
        QSKIP("requires Xcode profile");

    // XCode 11 produces warning about deprecation of 32-bit apps, but still works
    const bool hasX86Mac = xcodeVersion < qbs::Version(12);
    const bool hasArmMac = xcodeVersion >= qbs::Version(12, 2);

    QDir::setCurrent(testDataDir + "/aggregateDependencyLinking");
    QbsRunParameters params{QStringList{"-p", "multi_arch_lib"}};
    params.arguments << QStringLiteral("products.multi_arch_lib.hasX86Mac:%1").arg(hasX86Mac);
    params.arguments << QStringLiteral("products.multi_arch_lib.hasArmMac:%1").arg(hasArmMac);
    QCOMPARE(runQbs(params), 0);
    if (m_qbsStdout.contains("Cannot build fat binaries"))
        QSKIP("Building fat binaries is not supported for this profile");

    QCOMPARE(runQbs(QStringList{"-p", "just_app", "--command-echo-mode", "command-line"}), 0);
    int linkedInLibrariesCount =
            QString::fromUtf8(m_qbsStdout).count(QStringLiteral("libmulti_arch_lib.a"));
    QCOMPARE(linkedInLibrariesCount, 1);
    const auto fatLibPath = findFatLibrary(testDataDir, QStringLiteral("libmulti_arch_lib.a"));
    QVERIFY(!fatLibPath.isEmpty());
    QVERIFY2(QString::fromUtf8(m_qbsStdout).contains(fatLibPath), m_qbsStdout);
}

void TestBlackboxApple::appiconset()
{
    QDir::setCurrent(testDataDir + QLatin1String("/ib/appiconset"));

    QbsRunParameters params;
    params.arguments = QStringList() << "-f" << "appiconset.qbs";
    QCOMPARE(runQbs(params), 0);

    const auto infoPlistPath = getInfoPlistPath(
            relativeProductBuildDir("appiconset") + "/appiconset.app");
    QVERIFY(QFile::exists(infoPlistPath));
    const auto content = readInfoPlistFile(infoPlistPath);
    QVERIFY(!content.isEmpty());

    if (m_qbsStdout.contains("bundle.isShallow: false")) {
        QCOMPARE(content.value(QStringLiteral("CFBundleIconFile")), QStringLiteral("AppIcon"));
        QCOMPARE(content.value(QStringLiteral("CFBundleIconName")), QStringLiteral("AppIcon"));
        QVERIFY(regularFileExists(relativeProductBuildDir("appiconset")
                                  + "/appiconset.app/Contents/Resources/AppIcon.icns"));
    } else if (m_qbsStdout.contains("bundle.isShallow: true")) {
        const auto icons = content.value(QStringLiteral("CFBundleIcons")).toMap();
        QVERIFY2(!icons.isEmpty(), "Info.plist doesn't contain CFBundleIcons key");
        const auto primaryIcon = icons.value(QStringLiteral("CFBundlePrimaryIcon")).toMap();
        QVERIFY2(!primaryIcon.isEmpty(), "Info.plist doesn't contain CFBundlePrimaryIcon key");
        QCOMPARE(primaryIcon.value(QStringLiteral("CFBundleIconName")), QStringLiteral("AppIcon"));
    } else {
        QVERIFY2(false, "Cannot determine bundle type");
    }
}

void TestBlackboxApple::assetCatalog()
{
    QFETCH(bool, flatten);

    QDir::setCurrent(testDataDir + QLatin1String("/ib/assetcatalog"));

    rmDirR(relativeBuildDir());

    if (!findXcode())
        QSKIP("requires Xcode profile");

    QbsRunParameters params;
    const QString flattens = "modules.ib.flatten:" + QString(flatten ? "true" : "false");
    const QString macosTarget = "modules.cpp.minimumMacosVersion:'10.15'";

    // Make sure a dry run does not write anything
    params.arguments = QStringList() << "-f" << "assetcatalogempty.qbs" << "--dry-run"
                                     << flattens << macosTarget;
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!directoryExists(relativeBuildDir()));

    if (m_qbsStdout.contains("Skip this test"))
        QSKIP("Skip this test");

    params.arguments = QStringList() << "-f" << "assetcatalogempty.qbs"
                                     << flattens << macosTarget;
    QCOMPARE(runQbs(params), 0);

    // empty asset catalogs must still produce output
    QVERIFY((bool)m_qbsStdout.contains("compiling empty.xcassets"));

    // empty asset catalogs must still produce output
    QVERIFY((bool)m_qbsStdout.contains("compiling empty.xcassets"));
    QVERIFY(regularFileExists(relativeProductBuildDir("assetcatalogempty")
                              + "/assetcatalogempty.app/Contents/Resources/Assets.car"));

    // this asset catalog happens to have an embedded icon set,
    // but this should NOT be built since it is not in the files list
    QVERIFY(!(bool)m_qbsStdout.contains(".iconset"));

    // now we'll add the iconset
    rmDirR(relativeBuildDir());
    params.arguments.push_back("project.includeIconset:true");
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
    if (HostOsInfo::hostOsVersion() >= qbs::Version(10, 10)) {
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
    QCOMPARE(runQbs(QbsRunParameters("clean")), 0);
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
    if (findXcodeVersion() < qbs::Version(5))
        QSKIP("requires Xcode 5 or above");
    QDir::setCurrent(testDataDir + QLatin1String("/ib/empty-asset-catalogs"));
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling assetcatalog1.xcassets"), m_qbsStdout);
    QVERIFY2(!m_qbsStdout.contains("compiling assetcatalog2.xcassets"), m_qbsStdout);
}

void TestBlackboxApple::assetCatalogsMultiple() {
    if (findXcodeVersion() < qbs::Version(5))
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

    QDir::setCurrent(testDataDir + "/bundle-structure");
    QbsRunParameters params(QStringList{"qbs.installPrefix:''"});
    params.arguments << "project.buildableProducts:" + productName;

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

    if (m_qbsStdout.contains("bundle.isShallow: false")) {
        // Test shallow bundles detection - bundles are not shallow only on macOS, so also check
        // the qbs.targetOS property
        QVERIFY2(m_qbsStdout.contains("qbs.targetOS: macos"), m_qbsStdout);
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
    } else if (m_qbsStdout.contains("bundle.isShallow: true")) {
        QVERIFY2(m_qbsStdout.contains("qbs.targetOS:"), m_qbsStdout);
        QVERIFY2(!m_qbsStdout.contains("qbs.targetOS: macos"), m_qbsStdout);
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
    } else {
        QVERIFY2(false, qPrintable(m_qbsStdout));
    }
}

void TestBlackboxApple::bundleStructure_data()
{
    QTest::addColumn<QString>("productName");
    QTest::addColumn<QString>("productTypeIdentifier");
    QTest::addColumn<bool>("isShallow");

    QTest::newRow("A") << "A" << "com.apple.product-type.application";
    QTest::newRow("ABadApple") << "ABadApple" << "com.apple.product-type.will.never.exist.ever.guaranteed";
    QTest::newRow("ABadThirdParty") << "ABadThirdParty" << "org.special.third.party.non.existent.product.type";
    QTest::newRow("B") << "B" << "com.apple.product-type.framework";
    QTest::newRow("C") << "C" << "com.apple.product-type.framework.static";
    QTest::newRow("D") << "D" << "com.apple.product-type.bundle";
    QTest::newRow("E") << "E" << "com.apple.product-type.app-extension";
    QTest::newRow("F") << "F" << "com.apple.product-type.xpc-service";
    QTest::newRow("G") << "G" << "com.apple.product-type.in-app-purchase-content";
}

void TestBlackboxApple::byteArrayInfoPlist()
{
    QDir::setCurrent(testDataDir + "/byteArrayInfoPlist");

    QCOMPARE(runQbs(), 0);

    const auto infoPlistPath = getInfoPlistPath(
        relativeProductBuildDir("byteArrayInfoPlist") + "/byteArrayInfoPlist.app");
    QVERIFY(QFile::exists(infoPlistPath));
    const auto outFilePath =
        relativeProductBuildDir("byteArrayInfoPlist") + "/bytearrayInfoPlist-Info.plist.out";
    QFile file(outFilePath);
    QVERIFY(file.exists());
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), "The data value");
}

void TestBlackboxApple::codesign()
{
    QFETCH(bool, isBundle);
    QFETCH(bool, enableSigning);
    QFETCH(bool, multiArch);

    const auto xcodeVersion = findXcodeVersion();

    if (!xcodeVersion)
        QSKIP("requires Xcode profile");

    QDir::setCurrent(testDataDir + "/codesign");
    QbsRunParameters params(QStringList{"qbs.installPrefix:''"});
    params.arguments
            << QStringLiteral("project.isBundle:%1").arg(isBundle ? "true" : "false");
    params.arguments
            << QStringLiteral("project.enableSigning:%1").arg(enableSigning ? "true" : "false");
    if (multiArch)
        params.arguments << QStringLiteral("project.xcodeVersion:") + xcodeVersion->toString();

    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(params), 0);

    const int codeSignCount =
            QString::fromUtf8(m_qbsStdout).count(QStringLiteral("codesign"));
    // We have 3 products, we have to sign each exactly once, even in the multiplexed case
    QCOMPARE(codeSignCount, enableSigning ? 3 : 0);

    const auto appName = isBundle ? QStringLiteral("A.app") : QStringLiteral("A");
    const auto appPath = defaultInstallRoot + "/" + appName;
    QVERIFY(QFileInfo(appPath).exists());
    auto codeSignInfo = getCodeSignInfo(appPath);
    QVERIFY(codeSignInfo.first != CodeSignResult::Failed);
    QCOMPARE(codeSignInfo.first == CodeSignResult::Signed, enableSigning);
    QCOMPARE(codeSignInfo.second.isEmpty(), !enableSigning);
    if (!codeSignInfo.second.isEmpty()) {
        QVERIFY(codeSignInfo.second.contains(QByteArrayLiteral("Executable")));
        QVERIFY(codeSignInfo.second.contains(QByteArrayLiteral("Identifier")));
        QCOMPARE(codeSignInfo.second.value(QByteArrayLiteral("Signature")), "adhoc");
    }

    const auto libName =
            isBundle ? QStringLiteral("B.framework") : QStringLiteral("libB.1.0.0.dylib");
    const auto libPath = defaultInstallRoot + "/" + libName;
    QVERIFY(QFileInfo(libPath).exists());
    codeSignInfo = getCodeSignInfo(libPath);
    QVERIFY(codeSignInfo.first != CodeSignResult::Failed);
    QCOMPARE(codeSignInfo.first == CodeSignResult::Signed, enableSigning);
    QCOMPARE(codeSignInfo.second.isEmpty(), !enableSigning);
    if (!codeSignInfo.second.isEmpty()) {
        QVERIFY(codeSignInfo.second.contains(QByteArrayLiteral("Executable")));
        QVERIFY(codeSignInfo.second.contains(QByteArrayLiteral("Identifier")));
        QCOMPARE(codeSignInfo.second.value(QByteArrayLiteral("Signature")), "adhoc");
    }

    const auto pluginPath = defaultInstallRoot + "/" + QStringLiteral("C.bundle");
    QVERIFY(QFileInfo(pluginPath).exists());
    QVERIFY(QFileInfo(pluginPath).isDir() == isBundle);
    codeSignInfo = getCodeSignInfo(pluginPath);
    QVERIFY(codeSignInfo.first != CodeSignResult::Failed);
    QCOMPARE(codeSignInfo.first == CodeSignResult::Signed, enableSigning);
    QCOMPARE(codeSignInfo.second.isEmpty(), !enableSigning);
    if (!codeSignInfo.second.isEmpty()) {
        QVERIFY(codeSignInfo.second.contains(QByteArrayLiteral("Executable")));
        QVERIFY(codeSignInfo.second.contains(QByteArrayLiteral("Identifier")));
        QCOMPARE(codeSignInfo.second.value(QByteArrayLiteral("Signature")), "adhoc");
    }
}

void TestBlackboxApple::codesign_data()
{
    QTest::addColumn<bool>("isBundle");
    QTest::addColumn<bool>("enableSigning");
    QTest::addColumn<bool>("multiArch");

    QTest::newRow("bundle, unsigned") << true << false << false;
    QTest::newRow("standalone, unsigned") << false << false << false;
    QTest::newRow("bundle, signed") << true << true << false;
    QTest::newRow("standalone, signed") << false << true << false;
    QTest::newRow("bundle, signed, multiarch") << true << true << true;
    QTest::newRow("standalone, signed, multiarch") << false << true << true;
}

void TestBlackboxApple::deploymentTarget()
{
    QFETCH(QString, sdk);
    QFETCH(QString, os);
    QFETCH(QString, arch);
    QFETCH(QString, cflags);
    QFETCH(QString, lflags);

    QDir::setCurrent(testDataDir + "/deploymentTarget");

    if (!findXcode())
        QSKIP("requires Xcode profile");

    QbsRunParameters params;
    params.arguments = QStringList()
            << "--command-echo-mode"
            << "command-line"
            << "modules.qbs.targetPlatform:" + os
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
    static const QString macos = QStringLiteral("macos");
    static const QString ios = QStringLiteral("ios");
    static const QString ios_sim = QStringLiteral("ios-simulator");
    static const QString tvos = QStringLiteral("tvos");
    static const QString tvos_sim = QStringLiteral("tvos-simulator");
    static const QString watchos = QStringLiteral("watchos");
    static const QString watchos_sim = QStringLiteral("watchos-simulator");

    QTest::addColumn<QString>("sdk");
    QTest::addColumn<QString>("os");
    QTest::addColumn<QString>("arch");
    QTest::addColumn<QString>("cflags");
    QTest::addColumn<QString>("lflags");

    const auto xcodeVersion = findXcodeVersion();
    if (xcodeVersion < qbs::Version(10)) {
        QTest::newRow("macos x86") << "macosx" << macos << "x86"
                                   << "-triple i386-apple-macosx10.6"
                                   << "-macosx_version_min 10.6";
    }
    QTest::newRow("macos x86_64") << "macosx" << macos << "x86_64"
                         << "-triple x86_64-apple-macosx10.6"
                         << "10.6";

    if (xcodeVersion >= qbs::Version(6))
        QTest::newRow("macos x86_64h") << "macosx" << macos << "x86_64h"
                             << "-triple x86_64h-apple-macosx10.12"
                             << "10.12";

    QTest::newRow("ios armv7a") << "iphoneos" << ios << "armv7a"
                         << "-triple thumbv7-apple-ios6.0"
                         << "6.0";
    QTest::newRow("ios armv7s") << "iphoneos" <<ios << "armv7s"
                         << "-triple thumbv7s-apple-ios7.0"
                         << "7.0";
    if (xcodeVersion >= qbs::Version(5))
        QTest::newRow("ios arm64") << "iphoneos" <<ios << "arm64"
                             << "-triple arm64-apple-ios7.0"
                             << "7.0";
    QTest::newRow("ios-simulator x86") << "iphonesimulator" << ios_sim << "x86"
                             << "-triple i386-apple-ios6.0"
                             << "6.0";
    if (xcodeVersion >= qbs::Version(5))
        QTest::newRow("ios-simulator x86_64") << "iphonesimulator" << ios_sim << "x86_64"
                                 << "-triple x86_64-apple-ios7.0"
                                 << "7.0";

    if (xcodeVersion >= qbs::Version(7)) {
        if (xcodeVersion >= qbs::Version(7, 1)) {
            QTest::newRow("tvos arm64") << "appletvos" << tvos << "arm64"
                                 << "-triple arm64-apple-tvos9.0"
                                 << "9.0";
            QTest::newRow("tvos-simulator x86_64") << "appletvsimulator" << tvos_sim << "x86_64"
                                     << "-triple x86_64-apple-tvos9.0"
                                     << "9.0";
        }

        QTest::newRow("watchos armv7k") << "watchos" << watchos << "armv7k"
                             << "-triple thumbv7k-apple-watchos2.0"
                             << "2.0";
        QTest::newRow("watchos-simulator x86") << "watchsimulator" << watchos_sim << "x86"
                                 << "-triple i386-apple-watchos2.0"
                                 << "2.0";
    }
}

void TestBlackboxApple::dmg()
{
    if (qEnvironmentVariableIsSet("GITHUB_ACTIONS"))
        QSKIP("Skip this test when running on GitHub");
    QDir::setCurrent(testDataDir + "/apple-dmg");
    QCOMPARE(runQbs(), 0);
}

void TestBlackboxApple::embedInfoPlist()
{
    QDir::setCurrent(testDataDir + QLatin1String("/embedInfoPlist"));

    QbsRunParameters params(QStringList{"qbs.installPrefix:''"});
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

    if (m_qbsStdout.contains("isShallow: false")) {
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
    } else if (m_qbsStdout.contains("isShallow: true")) {
        QVERIFY(directoryExists(relativeProductBuildDir("Widget") + "/Widget.framework/Headers"));
        QVERIFY(directoryExists(relativeProductBuildDir("Widget") + "/Widget.framework/PrivateHeaders"));
        QVERIFY(regularFileExists(relativeProductBuildDir("Widget") + "/Widget.framework/Widget"));
        QVERIFY(regularFileExists(relativeProductBuildDir("Widget") + "/Widget.framework/Headers/Widget.h"));
        QVERIFY(regularFileExists(relativeProductBuildDir("Widget") + "/Widget.framework/PrivateHeaders/WidgetPrivate.h"));
        QVERIFY(regularFileExists(relativeProductBuildDir("Widget") + "/Widget.framework/BaseResource"));
        QVERIFY(regularFileExists(relativeProductBuildDir("Widget") + "/Widget.framework/en.lproj/EnglishResource"));
        QVERIFY(regularFileExists(relativeProductBuildDir("Widget") + "/Widget.framework/Widget"));
    } else {
        QVERIFY2(false, qPrintable(m_qbsStdout));
    }

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

    if (m_qbsStdout.contains("isShallow: false")) {
        QVERIFY(regularFileExists(relativeProductBuildDir("iconsetapp")
                                  + "/iconsetapp.app/Contents/Resources/white.icns"));
    } else if (m_qbsStdout.contains("isShallow: true")) {
        QVERIFY(regularFileExists(relativeProductBuildDir("iconsetapp")
                                  + "/iconsetapp.app/white.icns"));
    } else {
        QVERIFY2(false, qPrintable(m_qbsStdout));
    }
}

void TestBlackboxApple::infoPlist()
{
    QDir::setCurrent(testDataDir + "/infoplist");

    QbsRunParameters params;
    params.arguments = QStringList() << "-f" << "infoplist.qbs";
    QCOMPARE(runQbs(params), 0);

    const auto infoPlistPath = getInfoPlistPath(
            relativeProductBuildDir("infoplist") + "/infoplist.app");
    QVERIFY(QFile::exists(infoPlistPath));
    const auto content = readInfoPlistFile(infoPlistPath);
    QVERIFY(!content.isEmpty());

    // common values
    QCOMPARE(content.value(QStringLiteral("CFBundleIdentifier")),
             QStringLiteral("org.example.infoplist"));
    QCOMPARE(content.value(QStringLiteral("CFBundleName")), QStringLiteral("infoplist"));
    QCOMPARE(content.value(QStringLiteral("CFBundleExecutable")),
             QStringLiteral("infoplist"));

    if (!content.contains(QStringLiteral("SDKROOT"))) { // macOS-specific values
        QCOMPARE(content.value("LSMinimumSystemVersion"), QStringLiteral("10.7"));
        QCOMPARE(content.value("NSPrincipalClass"), QStringLiteral("NSApplication"));
        QCOMPARE(content.value(QStringLiteral("NSSupportsAutomaticGraphicsSwitching")), true);
    } else {
        // QBS-1447: UIDeviceFamily was set to a string instead of an array
        const auto family = content.value(QStringLiteral("UIDeviceFamily"));
        if (family.isValid()) {
            // Prior to Qt 5.15, int gets converted to a double when exporting plist as JSON
            QVERIFY(testVariantListType(family, QMetaType::LongLong)
                    || testVariantListType(family, QMetaType::Double));
        }
        const auto caps = content.value(QStringLiteral("UIRequiredDeviceCapabilities"));
        if (caps.isValid())
            QVERIFY(testVariantListType(caps, QMetaType::QString));
        const auto orientations = content.value(QStringLiteral("UIRequiredDeviceCapabilities"));
        if (orientations.isValid())
            QVERIFY(testVariantListType(orientations, QMetaType::QString));
    }
}

void TestBlackboxApple::infoPlistVariables()
{
    QDir::setCurrent(testDataDir + "/infoPlistVariables");

    QbsRunParameters params;
    params.arguments = QStringList() << "-f" << "infoPlistVariables.qbs";
    QCOMPARE(runQbs(params), 0);

    const auto infoPlistPath = getInfoPlistPath(
            relativeProductBuildDir("infoPlistVariables") + "/infoPlistVariables.app");
    QVERIFY(QFile::exists(infoPlistPath));
    const auto content = readInfoPlistFile(infoPlistPath);
    QVERIFY(!content.isEmpty());

    QCOMPARE(content.value(QStringLiteral("Curly")),
             QStringLiteral("infoPlistVariables"));
    QCOMPARE(content.value(QStringLiteral("Braces")),
             QStringLiteral("infoPlistVariables"));
    QCOMPARE(content.value(QStringLiteral("At")),
             QStringLiteral("infoPlistVariables"));
    QCOMPARE(content.value(QStringLiteral("CurlyMult")),
             QStringLiteral("infoPlistVariables_infoPlistVariables"));
    QCOMPARE(content.value(QStringLiteral("BracesMult")),
             QStringLiteral("infoPlistVariables_infoPlistVariables"));
    QCOMPARE(content.value(QStringLiteral("AtMult")),
             QStringLiteral("infoPlistVariables_infoPlistVariables"));
    QCOMPARE(content.value(QStringLiteral("CurlyNested")),
             QStringLiteral("infoPlistVariables"));
    QCOMPARE(content.value(QStringLiteral("BracesNested")),
             QStringLiteral("infoPlistVariables"));
    QCOMPARE(content.value(QStringLiteral("WithDefault")),
             QStringLiteral("DEFAULT"));
}

void TestBlackboxApple::objcArc()
{
    QDir::setCurrent(testDataDir + QLatin1String("/objc-arc"));

    QCOMPARE(runQbs(), 0);
}

void TestBlackboxApple::overrideInfoPlist()
{
    QDir::setCurrent(testDataDir + "/overrideInfoPlist");

    QCOMPARE(runQbs(), 0);

    const auto infoPlistPath = getInfoPlistPath(
            relativeProductBuildDir("overrideInfoPlist") + "/overrideInfoPlist.app");
    QVERIFY(QFile::exists(infoPlistPath));
    const auto content = readInfoPlistFile(infoPlistPath);
    QVERIFY(!content.isEmpty());

    // test we do not override custom values by default
    QCOMPARE(content.value(QStringLiteral("DefaultValue")),
             QStringLiteral("The default value"));
    // test we can override custom values
    QCOMPARE(content.value(QStringLiteral("OverriddenValue")),
             QStringLiteral("The overridden value"));
    // test we do not override special values set by Qbs by default
    QCOMPARE(content.value(QStringLiteral("CFBundleExecutable")),
             QStringLiteral("overrideInfoPlist"));
    // test we can override special values set by Qbs
    QCOMPARE(content.value(QStringLiteral("CFBundleName")), QStringLiteral("My Bundle"));
}

void TestBlackboxApple::xcode()
{
    if (!findXcode())
        QSKIP("requires Xcode profile");

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
    QVERIFY2(xcodebuildShowSdks.exitCode() == 0,
             qPrintable(xcodebuildShowSdks.readAllStandardError().constData()));
    const auto lines = QString::fromLocal8Bit(xcodebuildShowSdks.readAllStandardOutput().trimmed())
            .split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        static const std::regex regexp("^.+\\s+\\-sdk\\s+([a-z]+)([0-9]+\\.[0-9]+)$");
        const auto ln = line.toStdString();
        std::smatch match;
        if (std::regex_match(ln, match, regexp))
            sdks.insert({ match[1], match[2] });
    }

    const auto getSdksByType = [&sdks]()
    {
        QStringList result;
        std::string sdkType;
        QStringList sdkValues;
        for (const auto &sdk: sdks) {
            if (!sdkType.empty() && sdkType != sdk.first) {
                result.append(QStringLiteral("%1:['%2']")
                        .arg(QString::fromStdString(sdkType), sdkValues.join("','")));
                sdkValues.clear();
            }
            sdkType = sdk.first;
            sdkValues.append(QString::fromStdString(sdk.second));
        }
        return result;
    };

    QDir::setCurrent(testDataDir + "/xcode");
    QbsRunParameters params;
    params.arguments = (QStringList()
                        << (QStringLiteral("modules.xcode.developerPath:") + developerPath)
                        << (QStringLiteral("project.sdks:{") + getSdksByType().join(",") + "}"));
    QCOMPARE(runQbs(params), 0);
}

QTEST_MAIN(TestBlackboxApple)

std::optional<QVariantMap> TestBlackboxApple::findXcode(int *status)
{
    QTemporaryDir temp;
    QbsRunParameters params = QStringList({"-f", testDataDir + "/find/find-xcode.qbs"});
    params.buildDirectory = temp.path();
    const int res = runQbs(params);
    if (status)
        *status = res;
    QFile file(temp.path() + "/" + relativeProductBuildDir("find-xcode")
               + "/xcode.json");
    if (!file.open(QIODevice::ReadOnly))
        return {};
    auto result = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    if (!result["present"].toBool())
        return {};
    return result;
}

std::optional<qbs::Version> TestBlackboxApple::findXcodeVersion()
{
    const auto xcode = findXcode();
    if (!xcode)
        return {};
    return qbs::Version::fromString(xcode->value("version").toString());
}
