/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company.  For licensing terms and
** conditions see http://www.qt.io/terms-conditions.  For further information
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

#include "tst_blackbox.h"

#include "../shared.h"

#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <tools/installoptions.h>
#include <tools/profile.h>
#include <tools/settings.h>
#include <tools/shellutils.h>
#include <tools/version.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QRegExp>
#include <QTemporaryFile>

#define WAIT_FOR_NEW_TIMESTAMP() waitForNewTimestamp(testDataDir)

using qbs::InstallOptions;
using qbs::Internal::HostOsInfo;
using qbs::Internal::removeDirectoryWithContents;
using qbs::Profile;
using qbs::Settings;

class MacosTarHealer {
public:
    MacosTarHealer() {
        if (HostOsInfo::hostOs() == HostOsInfo::HostOsMacos) {
            // work around absurd tar behavior on macOS
            qputenv("COPY_EXTENDED_ATTRIBUTES_DISABLE", "true");
            qputenv("COPYFILE_DISABLE", "true");
        }
    }

    ~MacosTarHealer() {
        if (HostOsInfo::hostOs() == HostOsInfo::HostOsMacos) {
            qunsetenv("COPY_EXTENDED_ATTRIBUTES_DISABLE");
            qunsetenv("COPYFILE_DISABLE");
        }
    }
};

static QString initQbsExecutableFilePath()
{
    QString filePath = QCoreApplication::applicationDirPath() + QLatin1String("/qbs");
    filePath = HostOsInfo::appendExecutableSuffix(QDir::cleanPath(filePath));
    return filePath;
}

static bool supportsBuildDirectoryOption(const QString &command) {
    return !(QStringList() << "help" << "config" << "config-ui" << "qmltypes"
             << "setup-android" << "setup-qt" << "setup-toolchains").contains(command);
}

TestBlackbox::TestBlackbox()
    : testDataDir(testWorkDir(QStringLiteral("blackbox"))),
      testSourceDir(QDir::cleanPath(SRCDIR "/testdata")),
      qbsExecutableFilePath(initQbsExecutableFilePath()),
      defaultInstallRoot(relativeBuildDir() + QLatin1Char('/') + InstallOptions::defaultInstallRoot())
{
    QLocale::setDefault(QLocale::c());
}

int TestBlackbox::runQbs(const QbsRunParameters &params)
{
    QStringList args;
    if (!params.command.isEmpty())
        args << params.command;
    if (supportsBuildDirectoryOption(params.command)) {
        args.append(QLatin1String("-d"));
        args.append(params.buildDirectory.isEmpty() ? QLatin1String(".") : params.buildDirectory);
    }
    args << params.arguments;
    if (params.useProfile)
        args.append(QLatin1String("profile:") + profileName());
    QProcess process;
    process.setProcessEnvironment(params.environment);
    process.start(qbsExecutableFilePath, args);
    const int waitTime = 10 * 60000;
    if (!process.waitForStarted() || !process.waitForFinished(waitTime)) {
        m_qbsStderr = process.readAllStandardError();
        if (!params.expectFailure)
            qDebug("%s", qPrintable(process.errorString()));
        return -1;
    }

    m_qbsStderr = process.readAllStandardError();
    m_qbsStdout = process.readAllStandardOutput();
    sanitizeOutput(&m_qbsStderr);
    sanitizeOutput(&m_qbsStdout);
    if ((process.exitStatus() != QProcess::NormalExit
             || process.exitCode() != 0) && !params.expectFailure) {
        if (!m_qbsStderr.isEmpty())
            qDebug("%s", m_qbsStderr.constData());
        if (!m_qbsStdout.isEmpty())
            qDebug("%s", m_qbsStdout.constData());
    }
    return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
}

/*!
  Recursive copy from directory to another.
  Note that this updates the file stamps on Linux but not on Windows.
  */
static void ccp(const QString &sourceDirPath, const QString &targetDirPath)
{
    QDir currentDir;
    QDirIterator dit(sourceDirPath, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    while (dit.hasNext()) {
        dit.next();
        const QString targetPath = targetDirPath + QLatin1Char('/') + dit.fileName();
        currentDir.mkpath(targetPath);
        ccp(dit.filePath(), targetPath);
    }

    QDirIterator fit(sourceDirPath, QDir::Files | QDir::Hidden);
    while (fit.hasNext()) {
        fit.next();
        const QString targetPath = targetDirPath + QLatin1Char('/') + fit.fileName();
        QFile::remove(targetPath);  // allowed to fail
        QVERIFY(QFile::copy(fit.filePath(), targetPath));
    }
}

void TestBlackbox::rmDirR(const QString &dir)
{
    QString errorMessage;
    removeDirectoryWithContents(dir, &errorMessage);
}

QByteArray TestBlackbox::unifiedLineEndings(const QByteArray &ba)
{
    if (HostOsInfo::isWindowsHost()) {
        QByteArray result;
        result.reserve(ba.size());
        for (int i = 0; i < ba.size(); ++i) {
            char c = ba.at(i);
            if (c != '\r')
                result.append(c);
        }
        return result;
    } else {
        return ba;
    }
}

void TestBlackbox::sanitizeOutput(QByteArray *ba)
{
    if (HostOsInfo::isWindowsHost())
        ba->replace('\r', "");
}

void TestBlackbox::initTestCase()
{
    QVERIFY(regularFileExists(qbsExecutableFilePath));

    Settings settings((QString()));
    if (!settings.profiles().contains(profileName()))
        QFAIL(QByteArray("The build profile '" + profileName().toLocal8Bit() +
                         "' could not be found. Please set it up on your machine."));

    Profile buildProfile(profileName(), &settings);
    QVariant qtBinPath = buildProfile.value(QLatin1String("Qt.core.binPath"));
    if (!qtBinPath.isValid())
        QFAIL(QByteArray("The build profile '" + profileName().toLocal8Bit() +
                         "' is not a valid Qt profile."));
    if (!QFile::exists(qtBinPath.toString()))
        QFAIL(QByteArray("The build profile '" + profileName().toLocal8Bit() +
                         "' points to an invalid Qt path."));

    // Initialize the test data directory.
    QVERIFY(testDataDir != testSourceDir);
    rmDirR(testDataDir);
    QDir().mkpath(testDataDir);
    ccp(testSourceDir, testDataDir);
}

static QString findExecutable(const QStringList &fileNames)
{
    const QStringList path = QString::fromLocal8Bit(qgetenv("PATH"))
            .split(HostOsInfo::pathListSeparator(), QString::SkipEmptyParts);

    foreach (const QString &fileName, fileNames) {
        QFileInfo fi(fileName);
        if (fi.isAbsolute())
            return fi.exists() ? fileName : QString();
        foreach (const QString &ppath, path) {
            const QString fullPath
                    = HostOsInfo::appendExecutableSuffix(ppath + QLatin1Char('/') + fileName);
            if (QFileInfo(fullPath).exists())
                return QDir::cleanPath(fullPath);
        }
    }
    return QString();
}

QMap<QString, QString> TestBlackbox::findAndroid(int *status)
{
    QTemporaryDir temp;
    QDir::setCurrent(testDataDir + "/find");
    QbsRunParameters params = QStringList() << "-f" << "find-android.qbs";
    params.buildDirectory = temp.path();
    const int res = runQbs(params);
    if (status)
        *status = res;
    QFile file(temp.path() + "/" + relativeProductBuildDir("find-android") + "/android.json");
    if (!file.open(QIODevice::ReadOnly))
        return QMap<QString, QString> { };
    const auto tools = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    return QMap<QString, QString> {
        {"sdk", QDir::fromNativeSeparators(tools["sdk"].toString())},
        {"ndk", QDir::fromNativeSeparators(tools["ndk"].toString())},
    };
}

QMap<QString, QString> TestBlackbox::findJdkTools(int *status)
{
    QTemporaryDir temp;
    QDir::setCurrent(testDataDir + "/find");
    QbsRunParameters params = QStringList() << "-f" << "find-jdk.qbs";
    params.buildDirectory = temp.path();
    const int res = runQbs(params);
    if (status)
        *status = res;
    QFile file(temp.path() + "/" + relativeProductBuildDir("find-jdk") + "/jdk.json");
    if (!file.open(QIODevice::ReadOnly))
        return QMap<QString, QString> { };
    const auto tools = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    return QMap<QString, QString> {
        {"java", QDir::fromNativeSeparators(tools["java"].toString())},
        {"javac", QDir::fromNativeSeparators(tools["javac"].toString())},
        {"jar", QDir::fromNativeSeparators(tools["jar"].toString())}
    };
}

QMap<QString, QString> TestBlackbox::findNodejs(int *status)
{
    QTemporaryDir temp;
    QDir::setCurrent(testDataDir + "/find");
    QbsRunParameters params = QStringList() << "-f" << "find-nodejs.qbs";
    params.buildDirectory = temp.path();
    const int res = runQbs(params);
    if (status)
        *status = res;
    QFile file(temp.path() + "/" + relativeProductBuildDir("find-nodejs") + "/nodejs.json");
    if (!file.open(QIODevice::ReadOnly))
        return QMap<QString, QString> { };
    const auto tools = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    return QMap<QString, QString> {
        {"node", QDir::fromNativeSeparators(tools["node"].toString())}
    };
}

QMap<QString, QString> TestBlackbox::findTypeScript(int *status)
{
    QTemporaryDir temp;
    QDir::setCurrent(testDataDir + "/find");
    QbsRunParameters params = QStringList() << "-f" << "find-typescript.qbs";
    params.buildDirectory = temp.path();
    const int res = runQbs(params);
    if (status)
        *status = res;
    QFile file(temp.path() + "/" + relativeProductBuildDir("find-typescript") + "/typescript.json");
    if (!file.open(QIODevice::ReadOnly))
        return QMap<QString, QString> { };
    const auto tools = QJsonDocument::fromJson(file.readAll()).toVariant().toMap();
    return QMap<QString, QString> {
        {"tsc", QDir::fromNativeSeparators(tools["tsc"].toString())}
    };
}

QString TestBlackbox::findArchiver(const QString &fileName, int *status)
{
    if (fileName == "jar")
        return findJdkTools(status)[fileName];

    QString binary = findExecutable(QStringList(fileName));
    if (binary.isEmpty()) {
        Settings s((QString()));
        Profile p(profileName(), &s);
        binary = findExecutable(p.value("archiver.command").toStringList());
    }
    return binary;
}

static bool isXcodeProfile(const QString &profileName)
{
    qbs::Settings settings((QString()));
    qbs::Profile profile(profileName, &settings);
    return profile.value("qbs.toolchain").toStringList().contains("xcode");
}

void TestBlackbox::sevenZip()
{
    QDir::setCurrent(testDataDir + "/archiver");
    QString binary = findArchiver("7z");
    if (binary.isEmpty())
        QSKIP("7zip not found");
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << "archiver.type:7zip")), 0);
    const QString outputFile = relativeProductBuildDir("archivable") + "/archivable.7z";
    QVERIFY2(regularFileExists(outputFile), qPrintable(outputFile));
    QProcess listContents;
    listContents.start(binary, QStringList() << "l" << outputFile);
    QVERIFY2(listContents.waitForStarted(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.waitForFinished(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.exitCode() == 0, listContents.readAllStandardError().constData());
    const QByteArray output = listContents.readAllStandardOutput();
    QVERIFY2(output.contains("2 files"), output.constData());
    QVERIFY2(output.contains("test.txt"), output.constData());
    QVERIFY2(output.contains("archivable.qbs"), output.constData());
}

void TestBlackbox::tar()
{
    if (HostOsInfo::hostOs() == HostOsInfo::HostOsWindows)
        QSKIP("Beware of the msys tar");
    MacosTarHealer tarHealer;
    QDir::setCurrent(testDataDir + "/archiver");
    QString binary = findArchiver("tar");
    if (binary.isEmpty())
        QSKIP("tar not found");
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << "archiver.type:tar")), 0);
    const QString outputFile = relativeProductBuildDir("archivable") + "/archivable.tar.gz";
    QVERIFY2(regularFileExists(outputFile), qPrintable(outputFile));
    QProcess listContents;
    listContents.start(binary, QStringList() << "tf" << outputFile);
    QVERIFY2(listContents.waitForStarted(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.waitForFinished(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.exitCode() == 0, listContents.readAllStandardError().constData());
    QFile listFile("list.txt");
    QVERIFY2(listFile.open(QIODevice::ReadOnly), qPrintable(listFile.errorString()));
    QCOMPARE(listContents.readAllStandardOutput(), listFile.readAll());
}

static QStringList sortedFileList(const QByteArray &ba)
{
    auto list = QString::fromUtf8(ba).split(QRegExp("[\r\n]"), QString::SkipEmptyParts);
    std::sort(list.begin(), list.end());
    return list;
}

void TestBlackbox::zip()
{
    QFETCH(QString, binaryName);
    int status = 0;
    const QString binary = findArchiver(binaryName, &status);
    QCOMPARE(status, 0);
    if (binary.isEmpty())
        QSKIP("zip tool not found");

    QDir::setCurrent(testDataDir + "/archiver");
    QbsRunParameters params(QStringList()
                            << "archiver.type:zip" << "archiver.command:" + binary);
    QCOMPARE(runQbs(params), 0);
    const QString outputFile = relativeProductBuildDir("archivable") + "/archivable.zip";
    QVERIFY2(regularFileExists(outputFile), qPrintable(outputFile));
    QProcess listContents;
    if (binaryName == "zip") {
        // zipinfo is part of Info-Zip
        listContents.start("zipinfo", QStringList() << "-1" << outputFile);
    } else {
        listContents.start(binary, QStringList() << "tf" << outputFile);
    }
    QVERIFY2(listContents.waitForStarted(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.waitForFinished(), qPrintable(listContents.errorString()));
    QVERIFY2(listContents.exitCode() == 0, listContents.readAllStandardError().constData());
    QFile listFile("list.txt");
    QVERIFY2(listFile.open(QIODevice::ReadOnly), qPrintable(listFile.errorString()));
    QCOMPARE(sortedFileList(listContents.readAllStandardOutput()),
             sortedFileList(listFile.readAll()));

    // Make sure the module is still loaded when the java/jar fallback is not available
    params.arguments << "java.jdkPath:/blubb";
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::zip_data()
{
    QTest::addColumn<QString>("binaryName");
    QTest::newRow("zip") << "zip";
    QTest::newRow("jar") << "jar";
}

void TestBlackbox::zipInvalid()
{
    QDir::setCurrent(testDataDir + "/archiver");
    QbsRunParameters params(QStringList() << "archiver.type:zip"
                            << "archiver.command:/bin/something");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("unrecognized archive tool: 'something'"), m_qbsStderr.constData());
}

void TestBlackbox::alwaysRun()
{
    QFETCH(QString, projectFile);

    QDir::setCurrent(testDataDir + "/always-run");
    rmDirR(relativeBuildDir());
    QbsRunParameters params("build", QStringList() << "-f" << projectFile);
    QCOMPARE(runQbs(params), 0);
    QVERIFY(projectFile.contains("transformer") == m_qbsStderr.contains("deprecated"));
    QVERIFY(m_qbsStdout.contains("yo"));
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("yo"));
    WAIT_FOR_NEW_TIMESTAMP();
    QFile f(projectFile);
    QVERIFY2(f.open(QIODevice::ReadWrite), qPrintable(f.errorString()));
    QByteArray content = f.readAll();
    content.replace("alwaysRun: false", "alwaysRun: true");
    f.resize(0);
    f.write(content);
    f.close();

    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("yo"));
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("yo"));
}

void TestBlackbox::alwaysRun_data()
{
    QTest::addColumn<QString>("projectFile");
    QTest::newRow("Transformer") << "transformer.qbs";
    QTest::newRow("Rule") << "rule.qbs";
}

void TestBlackbox::android()
{
    QFETCH(QString, projectDir);
    QFETCH(QStringList, productNames);
    QFETCH(QList<int>, apkFileCounts);

    int status;
    const auto androidPaths = findAndroid(&status);

    const auto ndkPath = androidPaths["ndk"];
    static const QStringList ndkSamplesDirs = QStringList() << "teapot" << "no-native";
    if (!ndkPath.isEmpty() && !QFileInfo(ndkPath + "/samples").isDir()
            && ndkSamplesDirs.contains(projectDir))
        QSKIP("NDK samples directory not present");

    QDir::setCurrent(testDataDir + "/android/" + projectDir);
    Settings s((QString()));
    Profile p("qbs_autotests-android", &s);
    if (!p.exists() || (status != 0 && !p.value("Android.sdk.ndkDir").isValid()))
        QSKIP("No suitable Android test profile");
    QbsRunParameters params(QStringList("profile:" + p.name())
                            << "Android.ndk.platform:android-21");
    params.useProfile = false;
    QCOMPARE(runQbs(params), 0);
    for (int i = 0; i < productNames.count(); ++i) {
        const QString productName = productNames.at(i);
        QVERIFY(m_qbsStdout.contains("Creating " + productName.toLocal8Bit() + ".apk"));
        const QString apkFilePath = relativeProductBuildDir(productName, p.name())
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
}

void TestBlackbox::android_data()
{
    QTest::addColumn<QString>("projectDir");
    QTest::addColumn<QStringList>("productNames");
    QTest::addColumn<QList<int>>("apkFileCounts");
    QTest::newRow("teapot") << "teapot" << QStringList("com.sample.teapot") << (QList<int>() << 25);
    QTest::newRow("no native") << "no-native"
            << QStringList("com.example.android.basicmediadecoder") << (QList<int>() << 22);
    QTest::newRow("multiple libs") << "multiple-libs-per-apk" << QStringList("twolibs")
                                   << (QList<int>() << 10);
    QTest::newRow("multiple apks") << "multiple-apks-per-project"
                                   << (QStringList() << "twolibs1" << "twolibs2")
                                   << (QList<int>() << 15 << 9);
}

void TestBlackbox::buildDirectories()
{
    const QString projectDir
            = QDir::cleanPath(testDataDir + QLatin1String("/build-directories"));
    const QString projectBuildDir = projectDir + '/' + relativeBuildDir();
    QDir::setCurrent(projectDir);
    QCOMPARE(runQbs(), 0);
    const QStringList outputLines
            = QString::fromLocal8Bit(m_qbsStdout.trimmed()).split('\n', QString::SkipEmptyParts);
    QVERIFY2(outputLines.contains(projectDir + '/' + relativeProductBuildDir("p1")),
             m_qbsStdout.constData());
    QVERIFY2(outputLines.contains(projectDir + '/' + relativeProductBuildDir("p2")),
             m_qbsStdout.constData());
    QVERIFY2(outputLines.contains(projectBuildDir), m_qbsStdout.constData());
    QVERIFY2(outputLines.contains(projectDir), m_qbsStdout.constData());
}

class QFileInfo2 : public QFileInfo {
public:
    QFileInfo2(const QString &path) : QFileInfo(path) { }
    bool isRegularFile() const { return isFile() && !isSymLink(); }
    bool isRegularDir() const { return isDir() && !isSymLink(); }
    bool isFileSymLink() const { return isFile() && isSymLink(); }
    bool isDirSymLink() const { return isDir() && isSymLink(); }
};

void TestBlackbox::bundleStructure()
{
    if (!HostOsInfo::isMacosHost())
        QSKIP("only applies on macOS");

    QFETCH(QString, productName);
    QFETCH(QString, productTypeIdentifier);
    QFETCH(bool, isShallow);

    QDir::setCurrent(testDataDir + "/bundle-structure");
    QbsRunParameters params;
    params.arguments << "project.buildableProducts:" + productName;
    if (isShallow) {
        // Coerce shallow bundles - don't set bundle.isShallow directly because we want to test the
        // automatic detection
        params.arguments
                << "qbs.targetOS:ios,darwin,bsd,unix"
                << "qbs.architecture:arm64";
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
            QVERIFY(QFileInfo2(defaultInstallRoot + "/A.app/ResourceRules.plist").isRegularFile());
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
            QVERIFY(QFileInfo2(defaultInstallRoot + "/B.framework/ResourceRules.plist").isRegularFile());
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
            QVERIFY(QFileInfo2(defaultInstallRoot + "/C.framework/ResourceRules.plist").isRegularFile());
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
            QVERIFY(QFileInfo2(defaultInstallRoot + "/D.bundle/ResourceRules.plist").isRegularFile());
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
            QVERIFY(QFileInfo2(defaultInstallRoot + "/E.appex/ResourceRules.plist").isRegularFile());
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
            QVERIFY(QFileInfo2(defaultInstallRoot + "/F.xpc/ResourceRules.plist").isRegularFile());
        }

        if (productName == "G") {
            QVERIFY(QFileInfo2(defaultInstallRoot + "/G").isRegularDir());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/G/ContentInfo.plist").isRegularFile());
            QVERIFY(QFileInfo2(defaultInstallRoot + "/G/Contents/resource.txt").isRegularFile());
        }
    }
}

void TestBlackbox::bundleStructure_data()
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

void TestBlackbox::changedFiles_data()
{
    QTest::addColumn<bool>("useChangedFilesForInitialBuild");
    QTest::newRow("initial build with changed files") << true;
    QTest::newRow("initial build without changed files") << false;
}

void TestBlackbox::changedFiles()
{
    QFETCH(bool, useChangedFilesForInitialBuild);

    QDir::setCurrent(testDataDir + "/changed-files");
    rmDirR(relativeBuildDir());
    const QString changedFile = QDir::cleanPath(QDir::currentPath() + "/file1.cpp");
    QbsRunParameters params1;
    if (useChangedFilesForInitialBuild)
        params1 = QbsRunParameters(QStringList("--changed-files") << changedFile);

    // Initial run: Build all files, even though only one of them was marked as changed
    //              (if --changed-files was used).
    QCOMPARE(runQbs(params1), 0);
    QCOMPARE(m_qbsStdout.count("compiling"), 3);
    QCOMPARE(m_qbsStdout.count("creating"), 3);

    WAIT_FOR_NEW_TIMESTAMP();
    touch(QDir::currentPath() + "/main.cpp");

    // Now only the file marked as changed must be compiled, even though it hasn't really
    // changed and another one has.
    QbsRunParameters params2(QStringList("--changed-files") << changedFile);
    QCOMPARE(runQbs(params2), 0);
    QCOMPARE(m_qbsStdout.count("compiling"), 1);
    QCOMPARE(m_qbsStdout.count("creating"), 1);
    QVERIFY2(m_qbsStdout.contains("file1.cpp"), m_qbsStdout.constData());
}

void TestBlackbox::changeInDisabledProduct()
{
    QDir::setCurrent(testDataDir + "/change-in-disabled-product");
    QCOMPARE(runQbs(), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    QFile projectFile("project.qbs");
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    QByteArray content = projectFile.readAll();
    content.replace("// 'test2.txt'", "'test2.txt'");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::changeInImportedFile()
{
    QDir::setCurrent(testDataDir + "/change-in-imported-file");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("old output"), m_qbsStdout.constData());

    WAIT_FOR_NEW_TIMESTAMP();
    QFile jsFile("prepare.js");
    QVERIFY2(jsFile.open(QIODevice::ReadWrite), qPrintable(jsFile.errorString()));
    QByteArray content = jsFile.readAll();
    content.replace("old output", "new output");
    jsFile.resize(0);
    jsFile.write(content);
    jsFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("new output"), m_qbsStdout.constData());

    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY2(jsFile.open(QIODevice::ReadWrite), qPrintable(jsFile.errorString()));
    jsFile.resize(0);
    jsFile.write(content);
    jsFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY2(!m_qbsStdout.contains("output"), m_qbsStdout.constData());
}

static QJsonObject findByName(const QJsonArray &objects, const QString &name)
{
    for (const QJsonValue v : objects) {
        if (!v.isObject())
            continue;
        QJsonObject obj = v.toObject();
        const QString objName = obj.value(QLatin1String("name")).toString();
        if (objName == name)
            return obj;
    }
    return QJsonObject();
}

void TestBlackbox::dependenciesProperty()
{
    QDir::setCurrent(testDataDir + QLatin1String("/dependenciesProperty"));
    QCOMPARE(runQbs(), 0);
    QFile depsFile(relativeProductBuildDir("product1") + QLatin1String("/product1.deps"));
    QVERIFY(depsFile.open(QFile::ReadOnly));

    QJsonParseError jsonerror;
    QJsonDocument jsondoc = QJsonDocument::fromJson(depsFile.readAll(), &jsonerror);
    if (jsonerror.error != QJsonParseError::NoError) {
        qDebug() << jsonerror.errorString();
        QFAIL("JSON parsing failed.");
    }
    QVERIFY(jsondoc.isArray());
    QJsonArray dependencies = jsondoc.array();
    QCOMPARE(dependencies.size(), 2);
    QJsonObject product2 = findByName(dependencies, QStringLiteral("product2"));
    QJsonArray product2_type = product2.value(QLatin1String("type")).toArray();
    QCOMPARE(product2_type.size(), 1);
    QCOMPARE(product2_type.first().toString(), QLatin1String("application"));
    QCOMPARE(product2.value(QLatin1String("narf")).toString(), QLatin1String("zort"));
    QJsonArray product2_deps = product2.value(QLatin1String("dependencies")).toArray();
    QVERIFY(!product2_deps.isEmpty());
    QJsonObject product2_qbs = findByName(product2_deps, QStringLiteral("qbs"));
    QVERIFY(!product2_qbs.isEmpty());
    QJsonObject product2_cpp = findByName(product2_deps, QStringLiteral("cpp"));
    QJsonArray product2_cpp_defines = product2_cpp.value(QLatin1String("defines")).toArray();
    QCOMPARE(product2_cpp_defines.size(), 1);
    QCOMPARE(product2_cpp_defines.first().toString(), QLatin1String("SMURF"));
}

void TestBlackbox::dependencyProfileMismatch()
{
    QDir::setCurrent(testDataDir + "/dependency-profile-mismatch");
    qbs::Settings s((QString()));
    qbs::Internal::TemporaryProfile depProfile("qbs_autotests_profileMismatch", &s);
    depProfile.p.setValue("qbs.architecture", "x86"); // Profiles must not be empty...
    s.sync();
    QbsRunParameters params(QStringList() << ("project.mainProfile:" + profileName())
                            << ("project.depProfile:" + depProfile.p.name()));
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains(profileName().toLocal8Bit() + "', which does not exist"),
             m_qbsStderr.constData());
}

void TestBlackbox::deploymentTarget()
{
    if (!HostOsInfo::isMacosHost())
        QSKIP("only applies on macOS");

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
            << "qbs.architecture:" + arch;

    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains(cflags.toLatin1()), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains(lflags.toLatin1()), m_qbsStdout.constData());
}

void TestBlackbox::deploymentTarget_data()
{
    QTest::addColumn<QString>("os");
    QTest::addColumn<QString>("arch");
    QTest::addColumn<QString>("cflags");
    QTest::addColumn<QString>("lflags");
    QTest::newRow("macos") << "macos,darwin,bsd,unix" << "x86_64"
                         << "-triple x86_64-apple-macosx10.4"
                         << "-macosx_version_min 10.4";
    QTest::newRow("ios") << "ios,darwin,bsd,unix" << "arm64"
                         << "-triple arm64-apple-ios5.0"
                         << "-iphoneos_version_min 5.0";
    QTest::newRow("ios-sim") << "ios-simulator,ios,darwin,bsd,unix" << "x86_64"
                             << "-triple x86_64-apple-ios5.0"
                             << "-ios_simulator_version_min 5.0";
}

void TestBlackbox::symlinkRemoval()
{
    if (HostOsInfo::isWindowsHost())
        QSKIP("No symlink support on Windows.");
    QDir::setCurrent(testDataDir + "/symlink-removal");
    QVERIFY(QDir::current().mkdir("dir1"));
    QVERIFY(QDir::current().mkdir("dir2"));
    QVERIFY(QFile::link("dir2", "dir1/broken-link"));
    QVERIFY(QFile::link(QFileInfo("dir2").absoluteFilePath(), "dir1/valid-link-to-dir"));
    QVERIFY(QFile::link(QFileInfo("symlink-removal.qbs").absoluteFilePath(),
                        "dir1/valid-link-to-file"));
    QCOMPARE(runQbs(), 0);
    QVERIFY(!QFile::exists("dir1"));
    QVERIFY(QFile::exists("dir2"));
    QVERIFY(QFile::exists("symlink-removal.qbs"));
}

void TestBlackbox::usingsAsSoleInputsNonMultiplexed()
{
    QDir::setCurrent(testDataDir + QLatin1String("/usings-as-sole-inputs-non-multiplexed"));
    QCOMPARE(runQbs(), 0);
    const QString p3BuildDir = relativeProductBuildDir("p3");
    QVERIFY(regularFileExists(p3BuildDir + "/custom1.out.plus"));
    QVERIFY(regularFileExists(p3BuildDir + "/custom2.out.plus"));
}

void TestBlackbox::versionScript()
{
    Settings settings((QString()));
    Profile buildProfile(profileName(), &settings);
    QStringList toolchain = buildProfile.value("qbs.toolchain").toStringList();
    if (!toolchain.contains("gcc") || targetOs() != HostOsInfo::HostOsLinux)
        QSKIP("version script test only applies to Linux");
    QDir::setCurrent(testDataDir + "/versionscript");
    QCOMPARE(runQbs(QbsRunParameters(QStringList("-q")
                                     << ("qbs.installRoot:" + QDir::currentPath()))), 0);
    const QString output = QString::fromLocal8Bit(m_qbsStderr);
    QRegExp pattern(".*---(.*)---.*");
    QVERIFY2(pattern.exactMatch(output), qPrintable(output));
    QCOMPARE(pattern.captureCount(), 1);
    const QString nmPath = pattern.capturedTexts().at(1);
    if (!QFile::exists(nmPath))
        QSKIP("Cannot check for symbol presence: No nm found.");
    QProcess nm;
    nm.start(nmPath, QStringList(QDir::currentPath() + "/libversionscript.so"));
    QVERIFY(nm.waitForStarted());
    QVERIFY(nm.waitForFinished());
    const QByteArray allSymbols = nm.readAllStandardOutput();
    QCOMPARE(nm.exitCode(), 0);
    QVERIFY2(allSymbols.contains("dummyLocal"), allSymbols.constData());
    QVERIFY2(allSymbols.contains("dummyGlobal"), allSymbols.constData());
    nm.start(nmPath, QStringList() << "-g" << QDir::currentPath() + "/libversionscript.so");
    QVERIFY(nm.waitForStarted());
    QVERIFY(nm.waitForFinished());
    const QByteArray globalSymbols = nm.readAllStandardOutput();
    QCOMPARE(nm.exitCode(), 0);
    QVERIFY2(!globalSymbols.contains("dummyLocal"), allSymbols.constData());
    QVERIFY2(globalSymbols.contains("dummyGlobal"), allSymbols.constData());
}

static bool symlinkExists(const QString &linkFilePath)
{
    return QFileInfo(linkFilePath).isSymLink();
}

void TestBlackbox::clean()
{
    const QString appObjectFilePath = relativeProductBuildDir("app") + "/.obj/" + inputDirHash(".")
            + objectFileName("/main.cpp", profileName());
    const QString appExeFilePath = relativeExecutableFilePath("app");
    const QString depObjectFilePath = relativeProductBuildDir("dep") + "/.obj/" + inputDirHash(".")
            + objectFileName("/dep.cpp", profileName());
    const QString depLibBase = relativeProductBuildDir("dep")
            + '/' + QBS_HOST_DYNAMICLIB_PREFIX + "dep";
    QString depLibFilePath;
    QStringList symlinks;
    if (qbs::Internal::HostOsInfo::isMacosHost()) {
        depLibFilePath = depLibBase + ".1.1.0" + QBS_HOST_DYNAMICLIB_SUFFIX;
        symlinks << depLibBase + ".1.1" + QBS_HOST_DYNAMICLIB_SUFFIX
                 << depLibBase + ".1"  + QBS_HOST_DYNAMICLIB_SUFFIX
                 << depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX;
    } else if (qbs::Internal::HostOsInfo::isAnyUnixHost()) {
        depLibFilePath = depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX + ".1.1.0";
        symlinks << depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX + ".1.1"
                 << depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX + ".1"
                 << depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX;
    } else {
        depLibFilePath = depLibBase + QBS_HOST_DYNAMICLIB_SUFFIX;
    }

    QDir::setCurrent(testDataDir + "/clean");

    // Default behavior: Remove all.
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("clean"))), 0);
    QVERIFY(!QFile(appObjectFilePath).exists());
    QVERIFY(!QFile(appExeFilePath).exists());
    QVERIFY(!QFile(depObjectFilePath).exists());
    QVERIFY(!QFile(depLibFilePath).exists());
    foreach (const QString &symLink, symlinks)
        QVERIFY2(!symlinkExists(symLink), qPrintable(symLink));

    // Remove all, with a forced re-resolve in between.
    // This checks that rescuable artifacts are also removed.
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << "cpp.optimization:none")), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QCOMPARE(runQbs(QbsRunParameters("resolve", QStringList() << "cpp.optimization:fast")), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QCOMPARE(runQbs(QbsRunParameters("clean")), 0);
    QVERIFY(!QFile(appObjectFilePath).exists());
    QVERIFY(!QFile(appExeFilePath).exists());
    QVERIFY(!QFile(depObjectFilePath).exists());
    QVERIFY(!QFile(depLibFilePath).exists());
    foreach (const QString &symLink, symlinks)
        QVERIFY2(!symlinkExists(symLink), qPrintable(symLink));

    // Dry run.
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("clean"), QStringList("-n"))), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QVERIFY(regularFileExists(depObjectFilePath));
    QVERIFY(regularFileExists(depLibFilePath));
    foreach (const QString &symLink, symlinks)
        QVERIFY2(symlinkExists(symLink), qPrintable(symLink));

    // Product-wise, dependency only.
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QVERIFY(regularFileExists(depObjectFilePath));
    QVERIFY(regularFileExists(depLibFilePath));
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("clean"), QStringList("-p") << "dep")), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QVERIFY(!QFile(depObjectFilePath).exists());
    QVERIFY(!QFile(depLibFilePath).exists());
    foreach (const QString &symLink, symlinks)
        QVERIFY2(!symlinkExists(symLink), qPrintable(symLink));

    // Product-wise, dependent product only.
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(appObjectFilePath));
    QVERIFY(regularFileExists(appExeFilePath));
    QVERIFY(regularFileExists(depObjectFilePath));
    QVERIFY(regularFileExists(depLibFilePath));
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("clean"), QStringList("-p") << "app")), 0);
    QVERIFY(!QFile(appObjectFilePath).exists());
    QVERIFY(!QFile(appExeFilePath).exists());
    QVERIFY(regularFileExists(depObjectFilePath));
    QVERIFY(regularFileExists(depLibFilePath));
    foreach (const QString &symLink, symlinks)
        QVERIFY2(symlinkExists(symLink), qPrintable(symLink));
}

void TestBlackbox::concurrentExecutor()
{
    QDir::setCurrent(testDataDir + "/concurrent-executor");
    QCOMPARE(runQbs(QStringList() << "-j" << "2"), 0);
    QVERIFY2(!m_qbsStderr.contains("ASSERT"), m_qbsStderr.constData());
}

void TestBlackbox::conditionalExport()
{
    QDir::setCurrent(testDataDir + "/conditional-export");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("missing define"), m_qbsStderr.constData());

    params.expectFailure = false;
    params.arguments << "project.enableExport:true";
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::conflictingArtifacts()
{
    QDir::setCurrent(testDataDir + "/conflicting-artifacts");
    QbsRunParameters params(QStringList() << "-n");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Conflicting artifacts"), m_qbsStderr.constData());
}

void TestBlackbox::dbusAdaptors()
{
    QDir::setCurrent(testDataDir + "/dbus-adaptors");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::dbusInterfaces()
{
    QDir::setCurrent(testDataDir + "/dbus-interfaces");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::renameDependency()
{
    QDir::setCurrent(testDataDir + "/renameDependency");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDataDir + "/renameDependency/work");
    QCOMPARE(runQbs(), 0);

    WAIT_FOR_NEW_TIMESTAMP();
    QFile::remove("lib.h");
    QFile::remove("lib.cpp");
    ccp("../after", ".");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
}

void TestBlackbox::separateDebugInfo()
{
    QDir::setCurrent(testDataDir + "/separate-debug-info");
    QCOMPARE(runQbs(), 0);

    Settings settings((QString()));
    Profile buildProfile(profileName(), &settings);
    QStringList toolchain = buildProfile.value("qbs.toolchain").toStringList();
    QStringList targetOS = buildProfile.value("qbs.targetOS").toStringList();
    if (targetOS.contains("darwin") || (targetOS.isEmpty() && HostOsInfo::isMacosHost())) {
        QVERIFY(directoryExists(relativeProductBuildDir("app1") + "/app1.app.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("app1")
            + "/app1.app.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("app1")
            + "/app1.app.dSYM/Contents/Resources/DWARF/app1"));
        QCOMPARE(QDir(relativeProductBuildDir("app1")
            + "/app1.app.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(!QFile::exists(relativeProductBuildDir("app2") + "/app2.app.dSYM"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("app3") + "/app3.app.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("app3")
            + "/app3.app/Contents/MacOS/app3.dwarf"));
        QVERIFY(directoryExists(relativeProductBuildDir("app4") + "/app4.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("app4")
            + "/app4.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("app4")
            + "/app4.dSYM/Contents/Resources/DWARF/app4"));
        QCOMPARE(QDir(relativeProductBuildDir("app4")
            + "/app4.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(regularFileExists(relativeProductBuildDir("app5") + "/app5.dwarf"));
        QVERIFY(directoryExists(relativeProductBuildDir("foo1") + "/foo1.framework.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("foo1")
            + "/foo1.framework.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("foo1")
            + "/foo1.framework.dSYM/Contents/Resources/DWARF/foo1"));
        QCOMPARE(QDir(relativeProductBuildDir("foo1")
            + "/foo1.framework.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(!QFile::exists(relativeProductBuildDir("foo2") + "/foo2.framework.dSYM"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("foo3") + "/foo3.framework.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("foo3")
            + "/foo3.framework/Versions/A/foo3.dwarf"));
        QVERIFY(directoryExists(relativeProductBuildDir("foo4") + "/libfoo4.dylib.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("foo4")
            + "/libfoo4.dylib.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("foo4")
            + "/libfoo4.dylib.dSYM/Contents/Resources/DWARF/libfoo4.dylib"));
        QCOMPARE(QDir(relativeProductBuildDir("foo4")
            + "/libfoo4.dylib.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(regularFileExists(relativeProductBuildDir("foo5") + "/libfoo5.dylib.dwarf"));
        QVERIFY(directoryExists(relativeProductBuildDir("bar1") + "/bar1.bundle.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("bar1")
            + "/bar1.bundle.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("bar1")
            + "/bar1.bundle.dSYM/Contents/Resources/DWARF/bar1"));
        QCOMPARE(QDir(relativeProductBuildDir("bar1")
            + "/bar1.bundle.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(!QFile::exists(relativeProductBuildDir("bar2") + "/bar2.bundle.dSYM"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("bar3") + "/bar3.bundle.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("bar3")
            + "/bar3.bundle/Contents/MacOS/bar3.dwarf"));
        QVERIFY(directoryExists(relativeProductBuildDir("bar4") + "/bar4.bundle.dSYM"));
        QVERIFY(regularFileExists(relativeProductBuildDir("bar4")
            + "/bar4.bundle.dSYM/Contents/Info.plist"));
        QVERIFY(regularFileExists(relativeProductBuildDir("bar4")
            + "/bar4.bundle.dSYM/Contents/Resources/DWARF/bar4.bundle"));
        QCOMPARE(QDir(relativeProductBuildDir("bar4")
            + "/bar4.bundle.dSYM/Contents/Resources/DWARF")
                .entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries).size(), 1);
        QVERIFY(regularFileExists(relativeProductBuildDir("bar5") + "/bar5.bundle.dwarf"));
    } else if (toolchain.contains("gcc")) {
        QVERIFY(QFile::exists(relativeProductBuildDir("app1") + "/app1.debug"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("app2") + "/app2.debug"));
        QVERIFY(QFile::exists(relativeProductBuildDir("foo1") + "/libfoo1.so.debug"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("foo2") + "/libfoo2.so.debug"));
        QVERIFY(QFile::exists(relativeProductBuildDir("bar1") + "/libbar1.so.debug"));
        QVERIFY(!QFile::exists(relativeProductBuildDir("bar2") + "/libbar2.so.debug"));
    } else if (toolchain.contains("msvc")) {
        QVERIFY(QFile::exists(relativeProductBuildDir("app1") + "/app1.pdb"));
        QVERIFY(QFile::exists(relativeProductBuildDir("foo1") + "/foo1.pdb"));
        QVERIFY(QFile::exists(relativeProductBuildDir("bar1") + "/bar1.pdb"));
        // MSVC's linker even creates a pdb file if /Z7 is passed to the compiler.
    } else {
        QSKIP("Unsupported toolchain. Skipping.");
    }
}

void TestBlackbox::track_qrc()
{
    QDir::setCurrent(testDataDir + "/qrc");
    QCOMPARE(runQbs(), 0);
    const QString fileName = relativeExecutableFilePath("i");
    QVERIFY2(regularFileExists(fileName), qPrintable(fileName));
    QDateTime dt = QFileInfo(fileName).lastModified();
    WAIT_FOR_NEW_TIMESTAMP();
    {
        QFile f("stuff.txt");
        f.remove();
        QVERIFY(f.open(QFile::WriteOnly));
        f.write("bla");
        f.close();
    }
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(fileName));
    QVERIFY(dt < QFileInfo(fileName).lastModified());
}

void TestBlackbox::track_qobject_change()
{
    QDir::setCurrent(testDataDir + "/trackQObjChange");
    copyFileAndUpdateTimestamp("bla_qobject.h", "bla.h");
    QCOMPARE(runQbs(), 0);
    const QString productFilePath = relativeExecutableFilePath("i");
    QVERIFY2(regularFileExists(productFilePath), qPrintable(productFilePath));
    QString moc_bla_objectFileName = relativeProductBuildDir("i") + "/.obj/"
            + inputDirHash("GeneratedFiles") + objectFileName("/moc_bla.cpp", profileName());
    QVERIFY2(regularFileExists(moc_bla_objectFileName), qPrintable(moc_bla_objectFileName));

    WAIT_FOR_NEW_TIMESTAMP();
    copyFileAndUpdateTimestamp("bla_noqobject.h", "bla.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(productFilePath));
    QVERIFY(!QFile(moc_bla_objectFileName).exists());
}

void TestBlackbox::trackAddFile()
{
    QProcess process;
    QList<QByteArray> output;
    QDir::setCurrent(testDataDir + "/trackAddFile");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDataDir + "/trackAddFile/work");
    QCOMPARE(runQbs(), 0);

    process.start(relativeExecutableFilePath("someapp"), QStringList());
    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY2(process.waitForFinished(), qPrintable(process.errorString()));
    QCOMPARE(process.exitCode(), 0);
    output = process.readAllStandardOutput().split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "Hello World!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "NARF!");
    QString unchangedObjectFile = relativeBuildDir()
            + objectFileName("/someapp/narf.cpp", profileName());
    QDateTime unchangedObjectFileTime1 = QFileInfo(unchangedObjectFile).lastModified();

    WAIT_FOR_NEW_TIMESTAMP();
    ccp("../after", ".");
    touch("project.qbs");
    touch("main.cpp");
    QCOMPARE(runQbs(), 0);

    process.start(relativeExecutableFilePath("someapp"), QStringList());
    QVERIFY(process.waitForStarted());
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitCode(), 0);
    output = process.readAllStandardOutput().split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "Hello World!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "NARF!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "ZORT!");

    // the object file of the untouched source should not have changed
    QDateTime unchangedObjectFileTime2 = QFileInfo(unchangedObjectFile).lastModified();
    QCOMPARE(unchangedObjectFileTime1, unchangedObjectFileTime2);
}

void TestBlackbox::trackExternalProductChanges()
{
    QDir::setCurrent(testDataDir + "/trackExternalProductChanges");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));

    QbsRunParameters params;
    params.environment.insert("QBS_TEST_PULL_IN_FILE_VIA_ENV", "1");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));

    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));

    WAIT_FOR_NEW_TIMESTAMP();
    QFile jsFile("fileList.js");
    QVERIFY(jsFile.open(QIODevice::ReadWrite));
    QByteArray jsCode = jsFile.readAll();
    jsCode.replace("return []", "return ['jsFileChange.cpp']");
    jsFile.resize(0);
    jsFile.write(jsCode);
    jsFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));

    rmDirR(relativeBuildDir());
    QVERIFY(jsFile.open(QIODevice::ReadWrite));
    jsCode = jsFile.readAll();
    jsCode.replace("['jsFileChange.cpp']", "[]");
    jsFile.resize(0);
    jsFile.write(jsCode);
    jsFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling fileExists.cpp"));

    QFile cppFile("fileExists.cpp");
    QVERIFY(cppFile.open(QIODevice::WriteOnly));
    cppFile.write("void fileExists() { }\n");
    cppFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("compiling main.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling environmentChange.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling jsFileChange.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling fileExists.cpp"));

    rmDirR(relativeBuildDir());
    Settings s((QString()));
    const Profile profile(profileName(), &s);
    const QStringList toolchainTypes = profile.value("qbs.toolchain").toStringList();
    if (!toolchainTypes.contains("gcc"))
        QSKIP("Need GCC-like compiler to run this test");
    params.environment = QProcessEnvironment::systemEnvironment();
    params.environment.insert("INCLUDE_PATH_TEST", "1");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("hiddenheaderqbs.h"), m_qbsStderr.constData());
    params.environment.insert("CPLUS_INCLUDE_PATH",
                              QDir::toNativeSeparators(QDir::currentPath() + "/hidden"));
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::trackGroupConditionChange()
{
    QbsRunParameters params;
    params.expectFailure = true;
    QDir::setCurrent(testDataDir + "/group-condition-change");
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("jibbetnich"));

    params.arguments = QStringList("project.kaputt:false");
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::trackRemoveFile()
{
    QProcess process;
    QList<QByteArray> output;
    QDir::setCurrent(testDataDir + "/trackAddFile");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    ccp("after", "work");
    QDir::setCurrent(testDataDir + "/trackAddFile/work");
    QCOMPARE(runQbs(), 0);

    process.start(relativeExecutableFilePath("someapp"), QStringList());
    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY2(process.waitForFinished(), qPrintable(process.errorString()));
    QCOMPARE(process.exitCode(), 0);
    output = process.readAllStandardOutput().split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "Hello World!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "NARF!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "ZORT!");
    QString unchangedObjectFile = relativeBuildDir()
            + objectFileName("/someapp/narf.cpp", profileName());
    QDateTime unchangedObjectFileTime1 = QFileInfo(unchangedObjectFile).lastModified();

    WAIT_FOR_NEW_TIMESTAMP();
    QFile::remove("project.qbs");
    QFile::remove("main.cpp");
    QFile::copy("../before/project.qbs", "project.qbs");
    QFile::copy("../before/main.cpp", "main.cpp");
    QVERIFY(QFile::remove("zort.h"));
    QVERIFY(QFile::remove("zort.cpp"));
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("resolve"))), 0);

    touch("main.cpp");
    touch("project.qbs");
    QCOMPARE(runQbs(), 0);

    process.start(relativeExecutableFilePath("someapp"), QStringList());
    QVERIFY(process.waitForStarted());
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitCode(), 0);
    output = process.readAllStandardOutput().split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "Hello World!");
    QCOMPARE(output.takeFirst().trimmed().constData(), "NARF!");

    // the object file of the untouched source should not have changed
    QDateTime unchangedObjectFileTime2 = QFileInfo(unchangedObjectFile).lastModified();
    QCOMPARE(unchangedObjectFileTime1, unchangedObjectFileTime2);

    // the object file for the removed cpp file should have vanished too
    QVERIFY(!regularFileExists(relativeBuildDir()
                               + objectFileName("/someapp/zort.cpp", profileName())));
}

void TestBlackbox::trackAddFileTag()
{
    QProcess process;
    QList<QByteArray> output;
    QDir::setCurrent(testDataDir + "/trackFileTags");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDataDir + "/trackFileTags/work");
    QCOMPARE(runQbs(), 0);

    process.start(relativeExecutableFilePath("someapp"), QStringList());
    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY2(process.waitForFinished(), qPrintable(process.errorString()));
    QCOMPARE(process.exitCode(), 0);
    output = process.readAllStandardOutput().split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "there's no foo here");

    WAIT_FOR_NEW_TIMESTAMP();
    ccp("../after", ".");
    touch("main.cpp");
    touch("project.qbs");
    QCOMPARE(runQbs(), 0);

    process.start(relativeExecutableFilePath("someapp"), QStringList());
    QVERIFY(process.waitForStarted());
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitCode(), 0);
    output = process.readAllStandardOutput().split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "there's 15 foo here");
}

void TestBlackbox::trackRemoveFileTag()
{
    QProcess process;
    QList<QByteArray> output;
    QDir::setCurrent(testDataDir + "/trackFileTags");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("after", "work");
    QDir::setCurrent(testDataDir + "/trackFileTags/work");
    QCOMPARE(runQbs(), 0);

    // check if the artifacts are here that will become stale in the 2nd step
    QVERIFY(regularFileExists(relativeProductBuildDir("someapp") + "/.obj/" + inputDirHash(".")
                              + objectFileName("/main_foo.cpp", profileName())));
    QVERIFY(regularFileExists(relativeProductBuildDir("someapp") + "/main_foo.cpp"));
    QVERIFY(regularFileExists(relativeProductBuildDir("someapp") + "/main.foo"));

    process.start(relativeExecutableFilePath("someapp"), QStringList());
    QVERIFY(process.waitForStarted());
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitCode(), 0);
    output = process.readAllStandardOutput().split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "there's 15 foo here");

    WAIT_FOR_NEW_TIMESTAMP();
    ccp("../before", ".");
    touch("main.cpp");
    touch("project.qbs");
    QCOMPARE(runQbs(), 0);

    process.start(relativeExecutableFilePath("someapp"), QStringList());
    QVERIFY(process.waitForStarted());
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitCode(), 0);
    output = process.readAllStandardOutput().split('\n');
    QCOMPARE(output.takeFirst().trimmed().constData(), "there's no foo here");

    // check if stale artifacts have been removed
    QCOMPARE(regularFileExists(relativeProductBuildDir("someapp") + "/.obj/" + inputDirHash(".")
                               + objectFileName("/main_foo.cpp", profileName())), false);
    QCOMPARE(regularFileExists(relativeProductBuildDir("someapp") + "/main_foo.cpp"), false);
    QCOMPARE(regularFileExists(relativeProductBuildDir("someapp") + "/main.foo"), false);
}

void TestBlackbox::trackAddMocInclude()
{
    QDir::setCurrent(testDataDir + "/trackAddMocInclude");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDataDir + "/trackAddMocInclude/work");
    // The build must fail because the main.moc include is missing.
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);

    WAIT_FOR_NEW_TIMESTAMP();
    ccp("../after", ".");
    touch("main.cpp");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::trackAddProduct()
{
    QDir::setCurrent(testDataDir + "/trackProducts");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDataDir + "/trackProducts/work");
    QbsRunParameters params(QStringList() << "-f" << "trackProducts.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling foo.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling bar.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product1"));
    QVERIFY(m_qbsStdout.contains("linking product2"));

    WAIT_FOR_NEW_TIMESTAMP();
    ccp("../after", ".");
    touch("trackProducts.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling zoo.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product3"));
    QVERIFY(!m_qbsStdout.contains("compiling foo.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling bar.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product1"));
    QVERIFY(!m_qbsStdout.contains("linking product2"));
}

void TestBlackbox::trackRemoveProduct()
{
    QDir::setCurrent(testDataDir + "/trackProducts");
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    ccp("after", "work");
    QDir::setCurrent(testDataDir + "/trackProducts/work");
    QbsRunParameters params(QStringList() << "-f" << "trackProducts.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling foo.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling bar.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling zoo.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product1"));
    QVERIFY(m_qbsStdout.contains("linking product2"));
    QVERIFY(m_qbsStdout.contains("linking product3"));

    WAIT_FOR_NEW_TIMESTAMP();
    QFile::remove("zoo.cpp");
    QFile::remove("product3.qbs");
    copyFileAndUpdateTimestamp("../before/trackProducts.qbs", "trackProducts.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling foo.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling bar.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling zoo.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product1"));
    QVERIFY(!m_qbsStdout.contains("linking product2"));
    QVERIFY(!m_qbsStdout.contains("linking product3"));
}

void TestBlackbox::wildcardRenaming()
{
    QDir::setCurrent(testDataDir + "/wildcard_renaming");
    QCOMPARE(runQbs(QbsRunParameters("install")), 0);
    QVERIFY(QFileInfo(defaultInstallRoot + "/pioniere.txt").exists());
    QFile::rename(QDir::currentPath() + "/pioniere.txt", QDir::currentPath() + "/fdj.txt");
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("install"),
                                     QStringList("--clean-install-root"))), 0);
    QVERIFY(!QFileInfo(defaultInstallRoot + "/pioniere.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/fdj.txt").exists());
}

void TestBlackbox::recursiveRenaming()
{
    QDir::setCurrent(testDataDir + "/recursive_renaming");
    QCOMPARE(runQbs(QbsRunParameters("install")), 0);
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/wasser.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/subdir/blubb.txt").exists());
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(QFile::rename(QDir::currentPath() + "/dir/wasser.txt", QDir::currentPath() + "/dir/wein.txt"));
    QCOMPARE(runQbs(QbsRunParameters(QLatin1String("install"),
                                     QStringList("--clean-install-root"))), 0);
    QVERIFY(!QFileInfo(defaultInstallRoot + "/dir/wasser.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/wein.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/subdir/blubb.txt").exists());
}

void TestBlackbox::recursiveWildcards()
{
    QDir::setCurrent(testDataDir + "/recursive_wildcards");
    QCOMPARE(runQbs(QbsRunParameters("install")), 0);
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/file1.txt").exists());
    QVERIFY(QFileInfo(defaultInstallRoot + "/dir/file2.txt").exists());
}

void TestBlackbox::referenceErrorInExport()
{
    QDir::setCurrent(testDataDir + "/referenceErrorInExport");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QEXPECT_FAIL(0, "QBS-946", Abort);
    QVERIFY(m_qbsStderr.contains(
                "project.qbs:17:31 ReferenceError: Can't find variable: includePaths"));
}

void TestBlackbox::reproducibleBuild()
{
    Settings s((QString()));
    const Profile profile(profileName(), &s);
    const QStringList toolchains = profile.value("qbs.toolchain").toStringList();
    if (!toolchains.contains("gcc") || toolchains.contains("clang"))
        QSKIP("reproducible builds only supported for gcc");

    QFETCH(bool, reproducible);

    QDir::setCurrent(testDataDir + "/reproducible-build");
    QbsRunParameters params;
    params.arguments << QString("cpp.enableReproducibleBuilds:")
                        + (reproducible ? "true" : "false");
    QCOMPARE(runQbs(params), 0);
    QFile object(relativeProductBuildDir("the product") + "/.obj/" + inputDirHash(".") + '/'
                 + objectFileName("file1.cpp", profileName()));
    QVERIFY2(object.open(QIODevice::ReadOnly), qPrintable(object.fileName()));
    const QByteArray oldContents = object.readAll();
    object.close();
    QbsRunParameters cleanParams = params;
    cleanParams.command = "clean";
    QCOMPARE(runQbs(cleanParams), 0);
    QVERIFY(!object.exists());
    QCOMPARE(runQbs(params), 0);
    QVERIFY(object.open(QIODevice::ReadOnly));
    const QByteArray newContents = object.readAll();
    QCOMPARE(oldContents == newContents, reproducible);
    QCOMPARE(runQbs(cleanParams), 0);
}

void TestBlackbox::reproducibleBuild_data()
{
    QTest::addColumn<bool>("reproducible");
    QTest::newRow("non-reproducible build") << false;
    QTest::newRow("reproducible build") << true;
}

void TestBlackbox::responseFiles()
{
    QDir::setCurrent(testDataDir + "/response-files");
    QbsRunParameters params;
    params.command = "install";
    params.arguments << "--install-root" << "installed";
    QCOMPARE(runQbs(params), 0);
    QFile file("installed/response-file-content.txt");
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QList<QByteArray> expected = QList<QByteArray>()
            << "foo" <<  qbs::Internal::shellQuote("with space").toUtf8() << "bar" << "";
    QList<QByteArray> lines = file.readAll().split('\n');
    for (int i = 0; i < lines.count(); ++i)
        lines[i] = lines.at(i).trimmed();
    QCOMPARE(lines, expected);
}

void TestBlackbox::ruleConditions()
{
    QDir::setCurrent(testDataDir + "/ruleConditions");
    QCOMPARE(runQbs(), 0);
    QVERIFY(QFileInfo(relativeExecutableFilePath("zorted")).exists());
    QVERIFY(QFileInfo(relativeExecutableFilePath("unzorted")).exists());
    QVERIFY(QFileInfo(relativeProductBuildDir("zorted") + "/zorted.foo.narf.zort").exists());
    QVERIFY(!QFileInfo(relativeProductBuildDir("unzorted") + "/unzorted.foo.narf.zort").exists());
}

void TestBlackbox::ruleCycle()
{
    QDir::setCurrent(testDataDir + "/ruleCycle");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("Cycle detected in rule dependencies"));
}

void TestBlackbox::ruleWithNoInputs()
{
    QDir::setCurrent(testDataDir + "/rule-with-no-inputs");
    QVERIFY2(runQbs() == 0, m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("creating output"), m_qbsStdout.constData());
    QVERIFY2(runQbs() == 0, m_qbsStderr.constData());
    QVERIFY2(!m_qbsStdout.contains("creating output"), m_qbsStdout.constData());
    QbsRunParameters params(QStringList() << "theProduct.version:1");
    QVERIFY2(runQbs(params) == 0, m_qbsStderr.constData());
    QVERIFY2(!m_qbsStdout.contains("creating output"), m_qbsStdout.constData());
    params.arguments = QStringList() << "theProduct.version:2";
    QVERIFY2(runQbs(params) == 0, m_qbsStderr.constData());
    QVERIFY2(m_qbsStdout.contains("creating output"), m_qbsStdout.constData());
}

void TestBlackbox::overrideProjectProperties()
{
    QDir::setCurrent(testDataDir + "/overrideProjectProperties");
    QCOMPARE(runQbs(QbsRunParameters(QStringList()
                                     << QLatin1String("-f")
                                     << QLatin1String("project.qbs")
                                     << QLatin1String("project.nameSuffix:ForYou")
                                     << QLatin1String("project.someBool:false")
                                     << QLatin1String("project.someInt:156")
                                     << QLatin1String("project.someStringList:one")
                                     << QLatin1String("MyAppForYou.mainFile:main.cpp"))), 0);
    QVERIFY(regularFileExists(relativeExecutableFilePath("MyAppForYou")));
    QVERIFY(QFile::remove(relativeBuildGraphFilePath()));
    QbsRunParameters params;
    params.arguments << QLatin1String("-f") << QLatin1String("project_using_helper_lib.qbs");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);

    rmDirR(relativeBuildDir());
    params.arguments = QStringList() << QLatin1String("-f")
            << QLatin1String("project_using_helper_lib.qbs")
            << QLatin1String("project.linkSuccessfully:true");
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::pkgConfigProbe()
{
    const QString exe = findExecutable(QStringList() << "pkg-config");
    if (exe.isEmpty())
        QSKIP("This test requires the pkg-config tool");

    QDir::setCurrent(testDataDir + "/pkg-config-probe");

    QFETCH(QString, packageBaseName);
    QFETCH(QStringList, found);
    QFETCH(QStringList, libs);
    QFETCH(QStringList, cflags);
    QFETCH(QStringList, version);

    QbsRunParameters params(QStringList() << ("project.packageBaseName:" + packageBaseName));
    QCOMPARE(runQbs(params), 0);
    const QString stdOut = m_qbsStdout;
    QVERIFY2(stdOut.contains("theProduct1 found: " + found.at(0)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct2 found: " + found.at(1)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct1 libs: " + libs.at(0)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct2 libs: " + libs.at(1)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct1 cflags: " + cflags.at(0)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct2 cflags: " + cflags.at(1)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct1 version: " + version.at(0)), m_qbsStdout.constData());
    QVERIFY2(stdOut.contains("theProduct2 version: " + version.at(1)), m_qbsStdout.constData());
}

void TestBlackbox::pkgConfigProbe_data()
{
    QTest::addColumn<QString>("packageBaseName");
    QTest::addColumn<QStringList>("found");
    QTest::addColumn<QStringList>("libs");
    QTest::addColumn<QStringList>("cflags");
    QTest::addColumn<QStringList>("version");

    QTest::newRow("existing package")
            << "dummy" << (QStringList() << "true" << "true")
            << (QStringList() << "[\"-Ldummydir1\",\"-ldummy1\"]"
                << "[\"-Ldummydir2\",\"-ldummy2\"]")
            << (QStringList() << "[]" << "[]") << (QStringList() << "0.0.1" << "0.0.2");

    // Note: The array values should be "undefined", but we lose that information when
    //       converting to QVariants in the ProjectResolver.
    QTest::newRow("non-existing package")
            << "blubb" << (QStringList() << "false" << "false") << (QStringList() << "[]" << "[]")
            << (QStringList() << "[]" << "[]") << (QStringList() << "undefined" << "undefined");
}

void TestBlackbox::probeChangeTracking()
{
    QDir::setCurrent(testDataDir + "/probe-change-tracking");

    // Probe disabled.
    QbsRunParameters params;
    params.arguments = QStringList("theProduct.runProbe:false");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("running probe"));

    // Probe newly enabled.
    params.arguments = QStringList("theProduct.runProbe:true");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("running probe"));

    // Re-resolving with unchanged probe.
    WAIT_FOR_NEW_TIMESTAMP();
    QFile projectFile("probe-change-tracking.qbs");
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    QByteArray content = projectFile.readAll();
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(!m_qbsStdout.contains("running probe"));

    // Re-resolving with changed configure script.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    content = projectFile.readAll();
    content.replace("console.info", " console.info");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(m_qbsStdout.contains("running probe"));

    // Re-resolving with added property.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    content = projectFile.readAll();
    content.replace("condition: product.runProbe",
                    "condition: product.runProbe\nproperty string something: 'x'");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(m_qbsStdout.contains("running probe"));

    // Re-resolving with changed property.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    content = projectFile.readAll();
    content.replace("'x'", "'y'");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(m_qbsStdout.contains("running probe"));

    // Re-resolving with removed property.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    content = projectFile.readAll();
    content.replace("property string something: 'y'", "");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(m_qbsStdout.contains("running probe"));

    // Re-resolving with unchanged probe again.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    content = projectFile.readAll();
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(!m_qbsStdout.contains("running probe"));

    // Enforcing re-running via command-line option.
    params.arguments.prepend("--force-probe-execution");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Resolving"));
    QVERIFY(m_qbsStdout.contains("running probe"));
}

void TestBlackbox::probeProperties()
{
    QDir::setCurrent(testDataDir + "/probeProperties");
    const QByteArray dir = QDir::cleanPath(testDataDir).toLatin1() + "/probeProperties";
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("probe1.fileName=bin/tool"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe1.path=" + dir), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe1.filePath=" + dir + "/bin/tool"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe2.fileName=tool"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe2.path=" + dir + "/bin"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("probe2.filePath=" + dir + "/bin/tool"), m_qbsStdout.constData());
}

void TestBlackbox::probeInExportedModule()
{
    QDir::setCurrent(testDataDir + "/probe-in-exported-module");
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << QLatin1String("-f")
                                     << QLatin1String("probe-in-exported-module.qbs"))), 0);
    QVERIFY2(m_qbsStdout.contains("found: true"), m_qbsStdout.constData());
    QVERIFY2(m_qbsStdout.contains("prop: yes"), m_qbsStdout.constData());
}

void TestBlackbox::probesAndArrayProperties()
{
    QDir::setCurrent(testDataDir + "/probes-and-array-properties");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("prop: [\"probe\"]"), m_qbsStdout.constData());
    WAIT_FOR_NEW_TIMESTAMP();
    QFile projectFile("probes-and-array-properties.qbs");
    QVERIFY2(projectFile.open(QIODevice::ReadWrite), qPrintable(projectFile.errorString()));
    QByteArray content = projectFile.readAll();
    content.replace("//", "");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("prop: [\"product\",\"probe\"]"), m_qbsStdout.constData());
}

void TestBlackbox::productProperties()
{
    QDir::setCurrent(testDataDir + "/productproperties");
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << QLatin1String("-f")
                                     << QLatin1String("project.qbs"))), 0);
    QVERIFY(regularFileExists(relativeExecutableFilePath("blubb_user")));
}

void TestBlackbox::propertyChanges()
{
    QDir::setCurrent(testDataDir + "/propertyChanges");
    QFile projectFile("project.qbs");
    QbsRunParameters params(QStringList() << "-f" << "project.qbs");

    // Initial build.
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product 1.debug"));
    QVERIFY(m_qbsStdout.contains("generated.txt"));
    QVERIFY(m_qbsStdout.contains("Making output from input"));
    QVERIFY(m_qbsStdout.contains("Making output from other output"));
    QFile generatedFile(relativeProductBuildDir("generated text file") + "/generated.txt");
    QVERIFY(generatedFile.open(QIODevice::ReadOnly));
    QCOMPARE(generatedFile.readAll(), QByteArray("prefix 1contents 1suffix 1"));
    generatedFile.close();

    // Incremental build with no changes.
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));

    // Incremental build with no changes, but updated project file timestamp.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(projectFile.open(QIODevice::ReadWrite | QIODevice::Append));
    projectFile.write("\n");
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));

    // Incremental build, input property changed for first product
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    QByteArray contents = projectFile.readAll();
    contents.replace("blubb1", "blubb01");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product 1.debug"));
    QVERIFY(!m_qbsStdout.contains("linking product 2"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("linking library"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));

    // Incremental build, input property changed via project for second product.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    contents = projectFile.readAll();
    contents.replace("blubb2", "blubb02");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));

    // Incremental build, input property changed via command line for second product.
    params.arguments << "project.projectDefines:blubb002";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    params.arguments.removeLast();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));

    // Incremental build, input property changed via environment for third product.
    params.environment.insert("QBS_BLACKBOX_DEFINE", "newvalue");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(!m_qbsStdout.contains("linking product 2"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    params.environment.clear();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(!m_qbsStdout.contains("linking product 2"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));

    // Incremental build, module property changed via command line.
    params.arguments << "qbs.enableDebugCode:false";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product 1.release"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    params.arguments.removeLast();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(m_qbsStdout.contains("linking product 1.debug"));
    QVERIFY(m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));

    // Incremental build, non-essential dependency removed.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    contents = projectFile.readAll();
    contents.replace("Depends { name: 'library' }", "// Depends { name: 'library' }");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("linking product 1"));
    QVERIFY(m_qbsStdout.contains("linking product 2"));
    QVERIFY(!m_qbsStdout.contains("linking product 3"));
    QVERIFY(!m_qbsStdout.contains("linking library"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));

    // Incremental build, prepare script of a transformer changed.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    contents = projectFile.readAll();
    contents.replace("contents 1", "contents 2");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));
    QVERIFY(generatedFile.open(QIODevice::ReadOnly));
    QCOMPARE(generatedFile.readAll(), QByteArray("prefix 1contents 2suffix 1"));
    generatedFile.close();

    // Incremental build, product property used in JavaScript command changed.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    contents = projectFile.readAll();
    contents.replace("prefix 1", "prefix 2");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));
    QVERIFY(generatedFile.open(QIODevice::ReadOnly));
    QCOMPARE(generatedFile.readAll(), QByteArray("prefix 2contents 2suffix 1"));
    generatedFile.close();

    // Incremental build, project property used in JavaScript command changed.
    WAIT_FOR_NEW_TIMESTAMP();
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    contents = projectFile.readAll();
    contents.replace("suffix 1", "suffix 2");
    projectFile.resize(0);
    projectFile.write(contents);
    projectFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(m_qbsStdout.contains("generated.txt"));
    QVERIFY(!m_qbsStdout.contains("Making output from input"));
    QVERIFY(!m_qbsStdout.contains("Making output from other output"));
    QVERIFY(generatedFile.open(QIODevice::ReadOnly));
    QCOMPARE(generatedFile.readAll(), QByteArray("prefix 2contents 2suffix 2"));
    generatedFile.close();

    // Incremental build, prepare script of a rule in a module changed.
    WAIT_FOR_NEW_TIMESTAMP();
    QFile moduleFile("modules/TestModule/module.qbs");
    QVERIFY(moduleFile.open(QIODevice::ReadWrite));
    contents = moduleFile.readAll();
    contents.replace("// console.info('Change in source code')",
                     "console.info('Change in source code')");
    moduleFile.resize(0);
    moduleFile.write(contents);
    moduleFile.close();
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compiling source1.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source2.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling source3.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling lib.cpp"));
    QVERIFY(!m_qbsStdout.contains("generated.txt"));
    QVERIFY(m_qbsStdout.contains("Making output from input"));
    QVERIFY(m_qbsStdout.contains("Making output from other output"));
}

void TestBlackbox::qobjectInObjectiveCpp()
{
    if (!HostOsInfo::isMacosHost())
        QSKIP("only applies on macOS");
    const QString testDir = testDataDir + "/qobject-in-mm";
    QDir::setCurrent(testDir);
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::dynamicMultiplexRule()
{
    const QString testDir = testDataDir + "/dynamicMultiplexRule";
    QDir::setCurrent(testDir);
    QCOMPARE(runQbs(), 0);
    const QString outputFilePath = relativeProductBuildDir("dynamicMultiplexRule") + "/stuff-from-3-inputs";
    QVERIFY(regularFileExists(outputFilePath));
    WAIT_FOR_NEW_TIMESTAMP();
    touch("two.txt");
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(outputFilePath));
}

void TestBlackbox::dynamicRuleOutputs()
{
    const QString testDir = testDataDir + "/dynamicRuleOutputs";
    QDir::setCurrent(testDir);
    if (QFile::exists("work"))
        rmDirR("work");
    QDir().mkdir("work");
    ccp("before", "work");
    QDir::setCurrent(testDir + "/work");
    QCOMPARE(runQbs(), 0);

    const QString appFile = relativeExecutableFilePath("genlexer");
    const QString headerFile1 = relativeProductBuildDir("genlexer") + "/GeneratedFiles/numberscanner.h";
    const QString sourceFile1 = relativeProductBuildDir("genlexer") + "/GeneratedFiles/numberscanner.c";
    const QString sourceFile2 = relativeProductBuildDir("genlexer") + "/GeneratedFiles/lex.yy.c";

    // Check build #1: source and header file name are specified in numbers.l
    QVERIFY(regularFileExists(appFile));
    QVERIFY(regularFileExists(headerFile1));
    QVERIFY(regularFileExists(sourceFile1));
    QVERIFY(!QFile::exists(sourceFile2));

    QDateTime appFileTimeStamp1 = QFileInfo(appFile).lastModified();
    WAIT_FOR_NEW_TIMESTAMP();
    copyFileAndUpdateTimestamp("../after/numbers.l", "numbers.l");
    QCOMPARE(runQbs(), 0);

    // Check build #2: no file names are specified in numbers.l
    //                 flex will default to lex.yy.c without header file.
    QDateTime appFileTimeStamp2 = QFileInfo(appFile).lastModified();
    QVERIFY(appFileTimeStamp1 < appFileTimeStamp2);
    QVERIFY(!QFile::exists(headerFile1));
    QVERIFY(!QFile::exists(sourceFile1));
    QVERIFY(regularFileExists(sourceFile2));

    WAIT_FOR_NEW_TIMESTAMP();
    copyFileAndUpdateTimestamp("../before/numbers.l", "numbers.l");
    QCOMPARE(runQbs(), 0);

    // Check build #3: source and header file name are specified in numbers.l
    QDateTime appFileTimeStamp3 = QFileInfo(appFile).lastModified();
    QVERIFY(appFileTimeStamp2 < appFileTimeStamp3);
    QVERIFY(regularFileExists(appFile));
    QVERIFY(regularFileExists(headerFile1));
    QVERIFY(regularFileExists(sourceFile1));
    QVERIFY(!QFile::exists(sourceFile2));
}

void TestBlackbox::erroneousFiles_data()
{
    QTest::addColumn<QString>("errorMessage");
    QTest::newRow("nonexistentWorkingDir")
            << "The working directory '.does.not.exist' for process '.*ls' is invalid.";
}

void TestBlackbox::erroneousFiles()
{
    QFETCH(QString, errorMessage);
    QDir::setCurrent(testDataDir + "/erroneous/" + QTest::currentDataTag());
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QString err = QString::fromLocal8Bit(m_qbsStderr);
    if (!err.contains(QRegExp(errorMessage))) {
        qDebug() << "Output:  " << err;
        qDebug() << "Expected: " << errorMessage;
        QFAIL("Unexpected error message.");
    }
}

void TestBlackbox::errorInfo()
{
    QDir::setCurrent(testDataDir + "/error-info");
    QCOMPARE(runQbs(), 0);

    QbsRunParameters params;
    params.expectFailure = true;

    params.arguments = QStringList() << "project.fail1:true";
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("project.qbs:25"), m_qbsStderr);

    params.arguments = QStringList() << "project.fail2:true";
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("project.qbs:37"), m_qbsStderr);

    params.arguments = QStringList() << "project.fail3:true";
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("project.qbs:52"), m_qbsStderr);

    params.arguments = QStringList() << "project.fail4:true";
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("project.qbs:67"), m_qbsStderr);

    params.arguments = QStringList() << "project.fail5:true";
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("helper.js:4"), m_qbsStderr);

    params.arguments = QStringList() << "project.fail6:true";
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("helper.js:8"), m_qbsStderr);

    params.arguments = QStringList() << "project.fail7:true";
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("JavaScriptCommand.sourceCode"), m_qbsStderr);
    QVERIFY2(m_qbsStderr.contains("project.qbs:58"), m_qbsStderr);
}

void TestBlackbox::systemRunPaths()
{
    Settings settings((QString()));
    const Profile buildProfile(profileName(), &settings);
    switch (targetOs()) {
    case HostOsInfo::HostOsLinux:
    case HostOsInfo::HostOsMacos:
    case HostOsInfo::HostOsOtherUnix:
        break;
    default:
        QSKIP("only applies on Unix");
    }

    const QString lddFilePath = findExecutable(QStringList() << "ldd");
    if (lddFilePath.isEmpty())
        QSKIP("ldd not found");
    QDir::setCurrent(testDataDir + "/system-run-paths");
    QFETCH(bool, setRunPaths);
    QbsRunParameters params;
    params.arguments << QString("project.setRunPaths:") + (setRunPaths ? "true" : "false");
    QCOMPARE(runQbs(params), 0);
    QProcess ldd;
    ldd.start(lddFilePath, QStringList() << relativeExecutableFilePath("app"));
    QVERIFY2(ldd.waitForStarted(), qPrintable(ldd.errorString()));
    QVERIFY2(ldd.waitForFinished(), qPrintable(ldd.errorString()));
    QVERIFY2(ldd.exitCode() == 0, ldd.readAllStandardError().constData());
    const QByteArray output = ldd.readAllStandardOutput();
    const QList<QByteArray> outputLines = output.split('\n');
    QByteArray libLine;
    foreach (const auto &line, outputLines) {
        if (line.contains("theLib")) {
            libLine = line;
            break;
        }
    }
    QVERIFY2(!libLine.isEmpty(), output.constData());
    QVERIFY2(setRunPaths == libLine.contains("not found"), libLine.constData());
}

void TestBlackbox::systemRunPaths_data()
{
    QTest::addColumn<bool>("setRunPaths");
    QTest::newRow("do not set system run paths") << false;
    QTest::newRow("do set system run paths") << true;
}

void TestBlackbox::exportRule()
{
    QDir::setCurrent(testDataDir + "/export-rule");
    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::exportToOutsideSearchPath()
{
    QDir::setCurrent(testDataDir + "/export-to-outside-searchpath");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("Module 'aModule' not found when setting up transitive "
            "dependencies for product 'theProduct'"), m_qbsStderr.constData());
}

void TestBlackbox::fileDependencies()
{
    QDir::setCurrent(testDataDir + "/fileDependencies");
    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling narf.cpp"));
    QVERIFY(m_qbsStdout.contains("compiling zort.cpp"));
    const QString productFileName = relativeExecutableFilePath("myapp");
    QVERIFY2(regularFileExists(productFileName), qPrintable(productFileName));

    // Incremental build without changes.
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("compiling"));
    QVERIFY(!m_qbsStdout.contains("linking"));

    // Incremental build with changed file dependency.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("awesomelib/awesome.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling narf.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling zort.cpp"));

    // Incremental build with changed 2nd level file dependency.
    WAIT_FOR_NEW_TIMESTAMP();
    touch("awesomelib/magnificent.h");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("compiling narf.cpp"));
    QVERIFY(!m_qbsStdout.contains("compiling zort.cpp"));
}

void TestBlackbox::installedTransformerOutput()
{
    QDir::setCurrent(testDataDir + "/installed-transformer-output");
    QCOMPARE(runQbs(), 0);
    const QString installedFilePath = defaultInstallRoot + "/textfiles/HelloWorld.txt";
    QVERIFY2(QFile::exists(installedFilePath), qPrintable(installedFilePath));
}

void TestBlackbox::inputsFromDependencies()
{
    QDir::setCurrent(testDataDir + "/inputs-from-dependencies");
    QCOMPARE(runQbs(), 0);
    const QList<QByteArray> output = m_qbsStdout.trimmed().split('\n');
    QVERIFY2(output.contains((QDir::currentPath() + "/file1.txt").toUtf8()),
             m_qbsStdout.constData());
    QVERIFY2(output.contains((QDir::currentPath() + "/file2.txt").toUtf8()),
             m_qbsStdout.constData());
    QVERIFY2(output.contains((QDir::currentPath() + "/file3.txt").toUtf8()),
             m_qbsStdout.constData());
    QVERIFY2(!output.contains((QDir::currentPath() + "/file4.txt").toUtf8()),
             m_qbsStdout.constData());
}

void TestBlackbox::installPackage()
{
    if (HostOsInfo::hostOs() == HostOsInfo::HostOsWindows)
        QSKIP("Beware of the msys tar");
    QString binary = findArchiver("tar");
    if (binary.isEmpty())
        QSKIP("tar not found");
    MacosTarHealer tarHealer;
    QDir::setCurrent(testDataDir + "/installpackage");
    QCOMPARE(runQbs(), 0);
    const QString tarFilePath = relativeProductBuildDir("tar-package") + "/tar-package.tar.gz";
    QVERIFY2(regularFileExists(tarFilePath), qPrintable(tarFilePath));
    QProcess tarList;
    tarList.start(binary, QStringList() << "tf" << tarFilePath);
    QVERIFY2(tarList.waitForStarted(), qPrintable(tarList.errorString()));
    QVERIFY2(tarList.waitForFinished(), qPrintable(tarList.errorString()));
    const QList<QByteArray> outputLines = tarList.readAllStandardOutput().split('\n');
    QList<QByteArray> cleanOutputLines;
    foreach (const QByteArray &line, outputLines) {
        const QByteArray trimmedLine = line.trimmed();
        if (!trimmedLine.isEmpty())
            cleanOutputLines << trimmedLine;
    }
    QCOMPARE(cleanOutputLines.count(), 3);
    foreach (const QByteArray &line, cleanOutputLines) {
        QVERIFY2(line.contains("public_tool") || line.contains("mylib") || line.contains("lib.h"),
                 line.constData());
    }
}

void TestBlackbox::installable()
{
    QDir::setCurrent(testDataDir + "/installable");
    QCOMPARE(runQbs(), 0);
    QFile installList(relativeProductBuildDir("install-list") + "/installed-files.txt");
    QVERIFY2(installList.open(QIODevice::ReadOnly), qPrintable(installList.errorString()));
    QCOMPARE(installList.readAll().count('\n'), 2);
}

void TestBlackbox::installTree()
{
    QDir::setCurrent(testDataDir + "/install-tree");
    QbsRunParameters params;
    params.command = "install";
    QCOMPARE(runQbs(params), 0);
    const QString installRoot = relativeBuildDir() + "/install-root/";
    QVERIFY(QFile::exists(installRoot + "content/foo.txt"));
    QVERIFY(QFile::exists(installRoot + "content/subdir1/bar.txt"));
    QVERIFY(QFile::exists(installRoot + "content/subdir2/baz.txt"));
}

void TestBlackbox::invalidCommandProperty()
{
    QDir::setCurrent(testDataDir + "/invalid-command-property");
    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("unsuitable"), m_qbsStderr.constData());
}

static QProcessEnvironment processEnvironmentWithCurrentDirectoryInLibraryPath()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(HostOsInfo::libraryPathEnvironmentVariable(),
               (QStringList() << env.value(HostOsInfo::libraryPathEnvironmentVariable()) << ".")
               .join(HostOsInfo::pathListSeparator()));
    return env;
}

void TestBlackbox::java()
{
#if defined(Q_OS_WIN32) && !defined(Q_OS_WIN64)
    QSKIP("QTBUG-3845");
#endif

    Settings settings((QString()));
    Profile p(profileName(), &settings);

    int status;
    const auto jdkTools = findJdkTools(&status);
    QCOMPARE(status, 0);

    QDir::setCurrent(testDataDir + "/java");

    status = runQbs();
    if (p.value("java.jdkPath").toString().isEmpty()
            && status != 0 && m_qbsStderr.contains("jdkPath")) {
        QSKIP("java.jdkPath not set and automatic detection failed");
    }

    QCOMPARE(status, 0);

    const QStringList classFiles =
            QStringList() << "Jet" << "Ship" << "Vehicles";
    QStringList classFiles1 = QStringList(classFiles) << "io/qt/qbs/HelloWorld" << "NoPackage";
    for (int i = 0; i < classFiles1.count(); ++i) {
        QString &classFile = classFiles1[i];
        classFile = relativeProductBuildDir("class_collection") + "/classes/"
                + classFile + ".class";
        QVERIFY2(regularFileExists(classFile), qPrintable(classFile));
    }

    foreach (const QString &classFile, classFiles) {
        const QString filePath = relativeProductBuildDir("jar_file") + "/classes/" + classFile
                + ".class";
        QVERIFY2(regularFileExists(filePath), qPrintable(filePath));
    }
    const QString jarFilePath = relativeProductBuildDir("jar_file") + '/' + "jar_file.jar";
    QVERIFY2(regularFileExists(jarFilePath), qPrintable(jarFilePath));

    // Now check whether we correctly predicted the class file output paths.
    QCOMPARE(runQbs(QbsRunParameters("clean")), 0);
    foreach (const QString &classFile, classFiles1) {
        QVERIFY2(!regularFileExists(classFile), qPrintable(classFile));
    }

    // This tests various things: java.manifestClassPath, JNI, etc.
    QDir::setCurrent(relativeBuildDir() + "/install-root");
    QProcess process;
    process.setProcessEnvironment(processEnvironmentWithCurrentDirectoryInLibraryPath());
    process.start(HostOsInfo::appendExecutableSuffix(jdkTools["java"]),
            QStringList() << "-jar" << "jar_file.jar");
    if (process.waitForStarted()) {
        QVERIFY2(process.waitForFinished(), qPrintable(process.errorString()));
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
        const QByteArray stdOut = process.readAllStandardOutput();
        QVERIFY2(stdOut.contains("Driving!"), stdOut.constData());
        QVERIFY2(stdOut.contains("Flying!"), stdOut.constData());
        QVERIFY2(stdOut.contains("Flying (this is a space ship)!"), stdOut.constData());
        QVERIFY2(stdOut.contains("Sailing!"), stdOut.constData());
        QVERIFY2(stdOut.contains("Native code performing complex internal combustion process ("),
                 stdOut.constData());
    }

    process.start("unzip", QStringList() << "-p" << "jar_file.jar");
    if (process.waitForStarted()) {
        QVERIFY2(process.waitForFinished(), qPrintable(process.errorString()));
        const QByteArray stdOut = process.readAllStandardOutput();
        QVERIFY2(stdOut.contains("Class-Path: car_jar.jar random_stuff.jar"), stdOut.constData());
        QVERIFY2(stdOut.contains("Main-Class: Vehicles"), stdOut.constData());
    }
}

static QString dpkgArch(const QString &prefix = QString())
{
    QProcess dpkg;
    dpkg.start("/usr/bin/dpkg", QStringList() << "--print-architecture");
    dpkg.waitForFinished();
    if (dpkg.exitStatus() == QProcess::NormalExit && dpkg.exitCode() == 0)
        return prefix + QString::fromLocal8Bit(dpkg.readAllStandardOutput().trimmed());
    return QString();
}

void TestBlackbox::javaDependencyTracking() {
    Settings settings((QString()));
    Profile p(profileName(), &settings);

    auto getSpecificJdkVersion = [](const QString &jdkVersion) -> QString {
        if (HostOsInfo::isMacosHost()) {
            QProcess java_home;
            java_home.start("/usr/libexec/java_home", QStringList() << "--version" << jdkVersion);
            java_home.waitForFinished();
            if (java_home.exitStatus() == QProcess::NormalExit && java_home.exitCode() == 0)
                return QString::fromLocal8Bit(java_home.readAllStandardOutput().trimmed());
        } else if (HostOsInfo::isWindowsHost()) {
            QSettings settings("HKEY_LOCAL_MACHINE\\SOFTWARE\\JavaSoft\\Java Development Kit\\"
                               + jdkVersion, QSettings::NativeFormat);
            return settings.value("JavaHome").toString();
        } else {
            QString minorVersion = jdkVersion;
            if (minorVersion.startsWith("1."))
                minorVersion.remove(0, 2);

            const QStringList searchPaths = {
                "/usr/lib/jvm/java-" + minorVersion + "-openjdk" + dpkgArch("-"), // Debian
                "/usr/lib/jvm/java-" + minorVersion + "-openjdk", // Arch
                "/usr/lib/jvm/jre-1." + minorVersion + ".0-openjdk", // Fedora
            };
            for (const QString &searchPath : searchPaths) {
                if (QFile::exists(searchPath + "/bin/javac"))
                    return searchPath;
            }
        }

        return QString();
    };

    auto runQbsTest = [&](const QString &jdkPath, const QString &javaVersion,
            const QString &arg) {
        QDir::setCurrent(testDataDir + "/java");
        QbsRunParameters rp;
        rp.arguments.append(arg);
        if (!jdkPath.isEmpty())
            rp.arguments << ("java.jdkPath:" + jdkPath);
        if (!javaVersion.isEmpty())
            rp.arguments << ("java.languageVersion:" + javaVersion);
        rmDirR(relativeBuildDir());
        QCOMPARE(runQbs(rp), 0);
    };

    static const auto knownJdkVersions = QStringList() << "1.6" << "1.7" << "1.8" << "1.9"
                                                       << QString(); // default JDK;
    QStringList seenJdkVersions;
    for (const auto &jdkVersion : knownJdkVersions) {
        QString specificJdkPath = getSpecificJdkVersion(jdkVersion);
        if (jdkVersion.isEmpty() || !specificJdkPath.isEmpty()) {
            const auto jdkPath = jdkVersion.isEmpty() ? jdkVersion : specificJdkPath;

            if (!jdkVersion.isEmpty())
                seenJdkVersions << jdkVersion;

            if (!seenJdkVersions.isEmpty()) {
                const auto javaVersions = QStringList()
                    << knownJdkVersions.mid(0, knownJdkVersions.indexOf(seenJdkVersions.last()) + 1)
                    << QString(); // also test with no explicitly specified source version

                for (const auto &currentJavaVersion : javaVersions) {
                    runQbsTest(jdkPath, currentJavaVersion, "--check-outputs");
                    runQbsTest(jdkPath, currentJavaVersion, "--dry-run");
                }
            }
        }
    }

    if (seenJdkVersions.isEmpty())
        QSKIP("No JDKs installed");
}

void TestBlackbox::cli()
{
    QDir::setCurrent(testDataDir + "/cli");

    Settings s((QString()));
    Profile p("qbs_autotests-cli", &s);
    const QStringList toolchain = p.value("qbs.toolchain").toStringList();
    if (!p.exists() || !(toolchain.contains("dotnet") || toolchain.contains("mono")))
        QSKIP("No suitable Common Language Infrastructure test profile");

    QbsRunParameters params(QStringList() << "-f" << "dotnettest.qbs"
                            << "profile:" + p.name());
    params.useProfile = false;

    int status = runQbs(params);
    if (p.value("cli.toolchainInstallPath").toString().isEmpty()
            && status != 0 && m_qbsStderr.contains("toolchainInstallPath"))
        QSKIP("cli.toolchainInstallPath not set and automatic detection failed");

    QCOMPARE(status, 0);
    rmDirR(relativeBuildDir());

    QbsRunParameters params2(QStringList() << "-f" << "fshello.qbs"
                             << "profile:" + p.name());
    params2.useProfile = false;
    QCOMPARE(runQbs(params2), 0);
    rmDirR(relativeBuildDir());
}

void TestBlackbox::jsExtensionsFile()
{
    QDir::setCurrent(testDataDir + "/jsextensions-file");
    QbsRunParameters params(QStringList() << "-f" << "file.qbs");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!QFileInfo("original.txt").exists());
    QFile copy("copy.txt");
    QVERIFY(copy.exists());
    QVERIFY(copy.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = copy.readAll().trimmed().split('\n');
    QCOMPARE(lines.count(), 2);
    QCOMPARE(lines.at(0).trimmed().constData(), "false");
    QCOMPARE(lines.at(1).trimmed().constData(), "true");
}

void TestBlackbox::jsExtensionsFileInfo()
{
    QDir::setCurrent(testDataDir + "/jsextensions-fileinfo");
    QbsRunParameters params(QStringList() << "-f" << "fileinfo.qbs");
    QCOMPARE(runQbs(params), 0);
    QFile output("output.txt");
    QVERIFY(output.exists());
    QVERIFY(output.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = output.readAll().trimmed().split('\n');
    QCOMPARE(lines.count(), 23);
    int i = 0;
    QCOMPARE(lines.at(i++).trimmed().constData(), "blubb");
    QCOMPARE(lines.at(i++).trimmed().constData(), "blubb.tar");
    QCOMPARE(lines.at(i++).trimmed().constData(), "blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/tmp/blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "c:/tmp/blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "true");
    QCOMPARE(lines.at(i++).trimmed().constData(), HostOsInfo::isWindowsHost() ? "true" : "false");
    QCOMPARE(lines.at(i++).trimmed().constData(), "false");
    QCOMPARE(lines.at(i++).trimmed().constData(), "true");
    QCOMPARE(lines.at(i++).trimmed().constData(), "false");
    QCOMPARE(lines.at(i++).trimmed().constData(), "false");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/tmp/blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/tmp");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/tmp");
    QCOMPARE(lines.at(i++).trimmed().constData(), "/");
    QCOMPARE(lines.at(i++).trimmed().constData(), HostOsInfo::isWindowsHost() ? "d:/" : "d:");
    QCOMPARE(lines.at(i++).trimmed().constData(), "d:");
    QCOMPARE(lines.at(i++).trimmed().constData(), "d:/");
    QCOMPARE(lines.at(i++).trimmed().constData(), "blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "tmp/blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "../blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "\\tmp\\blubb.tar.gz");
    QCOMPARE(lines.at(i++).trimmed().constData(), "c:\\tmp\\blubb.tar.gz");
}

void TestBlackbox::jsExtensionsProcess()
{
    QDir::setCurrent(testDataDir + "/jsextensions-process");
    QbsRunParameters params(QStringList() << "-f" << "process.qbs" << "project.qbsFilePath:"
                            + qbsExecutableFilePath);
    QCOMPARE(runQbs(params), 0);
    QFile output("output.txt");
    QVERIFY(output.exists());
    QVERIFY(output.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = output.readAll().trimmed().split('\n');
    QCOMPARE(lines.count(), 8);
    QCOMPARE(lines.at(0).trimmed().constData(), "0");
    QVERIFY(lines.at(1).startsWith("qbs "));
    QCOMPARE(lines.at(2).trimmed().constData(), "true");
    QCOMPARE(lines.at(3).trimmed().constData(), "true");
    QCOMPARE(lines.at(4).trimmed().constData(), "0");
    QVERIFY(lines.at(5).startsWith("qbs "));
    QCOMPARE(lines.at(6).trimmed().constData(), "false");
    QCOMPARE(lines.at(7).trimmed().constData(), "should be");
}

void TestBlackbox::jsExtensionsPropertyList()
{
    if (!HostOsInfo::isMacosHost())
        QSKIP("temporarily only applies on macOS");

    QDir::setCurrent(testDataDir + "/jsextensions-propertylist");
    QbsRunParameters params(QStringList() << "-nf" << "propertylist.qbs");
    QCOMPARE(runQbs(params), 0);
    QFile file1("test.json");
    QVERIFY(file1.exists());
    QVERIFY(file1.open(QIODevice::ReadOnly));
    QFile file2("test.xml");
    QVERIFY(file2.exists());
    QVERIFY(file2.open(QIODevice::ReadOnly));
    QFile file3("test2.json");
    QVERIFY(file3.exists());
    QVERIFY(file3.open(QIODevice::ReadOnly));
    QByteArray file1Contents = file1.readAll();
    QCOMPARE(file3.readAll(), file1Contents);
    //QCOMPARE(file1Contents, file2.readAll()); // keys don't have guaranteed order
    QJsonParseError err1, err2;
    QCOMPARE(QJsonDocument::fromJson(file1Contents, &err1),
             QJsonDocument::fromJson(file2.readAll(), &err2));
    QVERIFY(err1.error == QJsonParseError::NoError && err2.error == QJsonParseError::NoError);
    QFile file4("test.openstep.plist");
    QVERIFY(file4.exists());
    QFile file5("test3.json");
    QVERIFY(file5.exists());
    QVERIFY(file5.open(QIODevice::ReadOnly));
    QVERIFY(file1Contents != file5.readAll());
}

void TestBlackbox::jsExtensionsTemporaryDir()
{
    QDir::setCurrent(testDataDir + "/jsextensions-temporarydir");
    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::jsExtensionsTextFile()
{
    QDir::setCurrent(testDataDir + "/jsextensions-textfile");
    QbsRunParameters params(QStringList() << "-f" << "textfile.qbs");
    QCOMPARE(runQbs(params), 0);
    QFile file1("file1.txt");
    QVERIFY(file1.exists());
    QVERIFY(file1.open(QIODevice::ReadOnly));
    QCOMPARE(file1.size(), qint64(0));
    QFile file2("file2.txt");
    QVERIFY(file2.exists());
    QVERIFY(file2.open(QIODevice::ReadOnly));
    const QList<QByteArray> lines = file2.readAll().trimmed().split('\n');
    QCOMPARE(lines.count(), 5);
    QCOMPARE(lines.at(0).trimmed().constData(), "false");
    QCOMPARE(lines.at(1).trimmed().constData(), "First line.");
    QCOMPARE(lines.at(2).trimmed().constData(), "Second line.");
    QCOMPARE(lines.at(3).trimmed().constData(), "Third line.");
    QCOMPARE(lines.at(4).trimmed().constData(), "true");
}

void TestBlackbox::ld()
{
    QDir::setCurrent(testDataDir + "/ld");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::linkerScripts()
{
    Settings settings((QString()));
    Profile buildProfile(profileName(), &settings);
    QStringList toolchain = buildProfile.value("qbs.toolchain").toStringList();
    if (!toolchain.contains("gcc") || targetOs() != HostOsInfo::HostOsLinux)
        QSKIP("linker script test only applies to Linux ");
    QDir::setCurrent(testDataDir + "/linkerscripts");
    QCOMPARE(runQbs(QbsRunParameters(QStringList("-q")
                                     << ("qbs.installRoot:" + QDir::currentPath()))), 0);
    const QString output = QString::fromLocal8Bit(m_qbsStderr);
    QRegExp pattern(".*---(.*)---.*");
    QVERIFY2(pattern.exactMatch(output), qPrintable(output));
    QCOMPARE(pattern.captureCount(), 1);
    const QString nmPath = pattern.capturedTexts().at(1);
    if (!QFile::exists(nmPath))
        QSKIP("Cannot check for symbol presence: No nm found.");
    QProcess nm;
    nm.start(nmPath, QStringList(QDir::currentPath() + "/liblinkerscripts.so"));
    QVERIFY(nm.waitForStarted());
    QVERIFY(nm.waitForFinished());
    const QByteArray nmOutput = nm.readAllStandardOutput();
    QCOMPARE(nm.exitCode(), 0);
    QVERIFY2(nmOutput.contains("TEST_SYMBOL1"), nmOutput.constData());
    QVERIFY2(nmOutput.contains("TEST_SYMBOL2"), nmOutput.constData());
}

void TestBlackbox::listPropertiesWithOuter()
{
    QDir::setCurrent(testDataDir + "/list-properties-with-outer");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("listProp: [\"product\",\"higher\",\"group\"]"),
             m_qbsStdout.constData());
}

void TestBlackbox::listPropertyOrder()
{
    QDir::setCurrent(testDataDir + "/list-property-order");
    const QbsRunParameters params(QStringList() << "-qq");
    QCOMPARE(runQbs(params), 0);
    const QByteArray firstOutput = m_qbsStderr;
    for (int i = 0; i < 25; ++i) {
        rmDirR(relativeBuildDir());
        QCOMPARE(runQbs(params), 0);
        if (m_qbsStderr != firstOutput)
            break;
    }
    QCOMPARE(m_qbsStderr.constData(), firstOutput.constData());
}

void TestBlackbox::mixedBuildVariants()
{
    QDir::setCurrent(testDataDir + "/mixed-build-variants");
    Settings settings((QString()));
    Profile profile(profileName(), &settings);
    if (profile.value("qbs.toolchain").toStringList().contains("msvc")) {
        QbsRunParameters params;
        params.expectFailure = true;
        QVERIFY(runQbs(params) != 0);
        QVERIFY2(m_qbsStderr.contains("not allowed"), m_qbsStderr.constData());
    } else if (!profile.value("Qt.core.availableBuildVariants").toStringList().contains("release")) {
        QbsRunParameters params;
        params.expectFailure = true;
        QVERIFY(runQbs(params) != 0);
        QVERIFY2(m_qbsStderr.contains("not supported"), m_qbsStderr.constData());
    } else {
        QCOMPARE(runQbs(), 0);
    }
}

void TestBlackbox::multipleChanges()
{
    QDir::setCurrent(testDataDir + "/multiple-changes");
    QCOMPARE(runQbs(), 0);
    QFile newFile("test.blubb");
    QVERIFY(newFile.open(QIODevice::WriteOnly));
    newFile.close();
    QCOMPARE(runQbs(QStringList() << "project.prop:true"), 0);
    QVERIFY(m_qbsStdout.contains("prop: true"));
}

void TestBlackbox::nestedProperties()
{
    QDir::setCurrent(testDataDir + "/nested-properties");
    QCOMPARE(runQbs(), 0);
    QVERIFY2(m_qbsStdout.contains("value in higherlevel"), m_qbsStdout.constData());
}

void TestBlackbox::newOutputArtifact()
{
    QDir::setCurrent(testDataDir + "/new-output-artifact");
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(relativeBuildDir() + "/install-root/output_98.out"));
    const QString the100thArtifact = relativeBuildDir() + "/install-root/output_99.out";
    QVERIFY(!regularFileExists(the100thArtifact));
    QbsRunParameters params(QStringList() << "theProduct.artifactCount:100");
    QCOMPARE(runQbs(params), 0);
    QVERIFY(regularFileExists(the100thArtifact));
}

void TestBlackbox::nonBrokenFilesInBrokenProduct()
{
    QDir::setCurrent(testDataDir + "/non-broken-files-in-broken-product");
    QbsRunParameters params(QStringList() << "-k");
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStdout.contains("fine.cpp"));
    QVERIFY(runQbs(params) != 0);
    QVERIFY(!m_qbsStdout.contains("fine.cpp")); // The non-broken file must not be recompiled.
}

void TestBlackbox::nonDefaultProduct()
{
    QDir::setCurrent(testDataDir + "/non-default-product");
    const QString defaultAppExe = relativeExecutableFilePath("default app");
    const QString nonDefaultAppExe = relativeExecutableFilePath("non-default app");

    QCOMPARE(runQbs(), 0);
    QVERIFY2(QFile::exists(defaultAppExe), qPrintable(defaultAppExe));
    QVERIFY2(!QFile::exists(nonDefaultAppExe), qPrintable(nonDefaultAppExe));

    QCOMPARE(runQbs(QbsRunParameters(QStringList() << "--all-products")), 0);
    QVERIFY2(QFile::exists(nonDefaultAppExe), qPrintable(nonDefaultAppExe));
}

static void switchProfileContents(qbs::Profile &p, Settings *s, bool on)
{
    const QString scalarKey = "leaf.scalarProp";
    const QString listKey = "leaf.listProp";
    if (on) {
        p.setValue(scalarKey, "profile");
        p.setValue(listKey, QStringList() << "profile");
    } else {
        p.remove(scalarKey);
        p.remove(listKey);
    }
    s->sync();
}

static void switchFileContents(QFile &f, bool on)
{
    f.seek(0);
    QByteArray contents = f.readAll();
    f.resize(0);
    if (on)
        contents.replace("// leaf.", "leaf.");
    else
        contents.replace("leaf.", "// leaf.");
    f.write(contents);
    f.flush();
}

void TestBlackbox::propertyPrecedence()
{
    QDir::setCurrent(testDataDir + "/property-precedence");
    qbs::Settings s((QString()));
    qbs::Internal::TemporaryProfile profile("qbs_autotests_propPrecedence", &s);
    profile.p.setValue("qbs.architecture", "x86"); // Profiles must not be empty...
    s.sync();
    const QStringList args = QStringList() << "-f" << "project.qbs"
                                           << ("profile:" + profile.p.name());
    QbsRunParameters params(args);
    params.useProfile = false;

    // Case 1: [cmdline=0,prod=0,export=0,nonleaf=0,profile=0]
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: leaf\n")
             && m_qbsStdout.contains("list prop: [\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 2: [cmdline=0,prod=0,export=0,nonleaf=0,profile=1]
    switchProfileContents(profile.p, &s, true);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: profile\n")
             && m_qbsStdout.contains("list prop: [\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 3: [cmdline=0,prod=0,export=0,nonleaf=1,profile=0]
    QFile nonleafFile("modules/nonleaf/nonleaf.qbs");
    QVERIFY2(nonleafFile.open(QIODevice::ReadWrite), qPrintable(nonleafFile.errorString()));
    switchProfileContents(profile.p, &s, false);
    switchFileContents(nonleafFile, true);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: nonleaf\n")
             && m_qbsStdout.contains("list prop: [\"nonleaf\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 4: [cmdline=0,prod=0,export=0,nonleaf=1,profile=1]
    switchProfileContents(profile.p, &s, true);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: nonleaf\n")
             && m_qbsStdout.contains("list prop: [\"nonleaf\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 5: [cmdline=0,prod=0,export=1,nonleaf=0,profile=0]
    QFile depFile("dep.qbs");
    QVERIFY2(depFile.open(QIODevice::ReadWrite), qPrintable(depFile.errorString()));
    switchProfileContents(profile.p, &s, false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, true);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: export\n")
             && m_qbsStdout.contains("list prop: [\"export\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 6: [cmdline=0,prod=0,export=1,nonleaf=0,profile=1]
    switchProfileContents(profile.p, &s, true);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: export\n")
             && m_qbsStdout.contains("list prop: [\"export\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 7: [cmdline=0,prod=0,export=1,nonleaf=1,profile=0]
    switchProfileContents(profile.p, &s, false);
    switchFileContents(nonleafFile, true);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: export\n")
             && m_qbsStdout.contains("list prop: [\"export\",\"nonleaf\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 8: [cmdline=0,prod=0,export=1,nonleaf=1,profile=1]
    switchProfileContents(profile.p, &s, true);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: export\n")
             && m_qbsStdout.contains("list prop: [\"export\",\"nonleaf\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 9: [cmdline=0,prod=1,export=0,nonleaf=0,profile=0]
    QFile productFile("project.qbs");
    QVERIFY2(productFile.open(QIODevice::ReadWrite), qPrintable(productFile.errorString()));
    switchProfileContents(profile.p, &s, false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, false);
    switchFileContents(productFile, true);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 10: [cmdline=0,prod=1,export=0,nonleaf=0,profile=1]
    switchProfileContents(profile.p, &s, true);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 11: [cmdline=0,prod=1,export=0,nonleaf=1,profile=0]
    switchProfileContents(profile.p, &s, false);
    switchFileContents(nonleafFile, true);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"nonleaf\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 12: [cmdline=0,prod=1,export=0,nonleaf=1,profile=1]
    switchProfileContents(profile.p, &s, true);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"nonleaf\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 13: [cmdline=0,prod=1,export=1,nonleaf=0,profile=0]
    switchProfileContents(profile.p, &s, false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, true);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"export\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 14: [cmdline=0,prod=1,export=1,nonleaf=0,profile=1]
    switchProfileContents(profile.p, &s, true);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"export\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Case 15: [cmdline=0,prod=1,export=1,nonleaf=1,profile=0]
    switchProfileContents(profile.p, &s, false);
    switchFileContents(nonleafFile, true);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"export\",\"nonleaf\",\"leaf\"]\n"),
             m_qbsStdout.constData());

    // Case 16: [cmdline=0,prod=1,export=1,nonleaf=1,profile=1]
    switchProfileContents(profile.p, &s, true);
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: product\n")
             && m_qbsStdout.contains("list prop: [\"product\",\"export\",\"nonleaf\",\"profile\"]\n"),
             m_qbsStdout.constData());

    // Command line properties wipe everything, including lists.
    // Case 17: [cmdline=1,prod=0,export=0,nonleaf=0,profile=0]
    switchProfileContents(profile.p, &s, false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, false);
    switchFileContents(productFile, false);
    params.arguments << "leaf.scalarProp:cmdline" << "leaf.listProp:cmdline";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 18: [cmdline=1,prod=0,export=0,nonleaf=0,profile=1]
    switchProfileContents(profile.p, &s, true);
    params.arguments << "leaf.scalarProp:cmdline" << "leaf.listProp:cmdline";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 19: [cmdline=1,prod=0,export=0,nonleaf=1,profile=0]
    switchProfileContents(profile.p, &s, false);
    switchFileContents(nonleafFile, true);
    params.arguments << "leaf.scalarProp:cmdline" << "leaf.listProp:cmdline";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 20: [cmdline=1,prod=0,export=0,nonleaf=1,profile=1]
    switchProfileContents(profile.p, &s, true);
    params.arguments << "leaf.scalarProp:cmdline" << "leaf.listProp:cmdline";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 21: [cmdline=1,prod=0,export=1,nonleaf=0,profile=0]
    switchProfileContents(profile.p, &s, false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, true);
    params.arguments << "leaf.scalarProp:cmdline" << "leaf.listProp:cmdline";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 22: [cmdline=1,prod=0,export=1,nonleaf=0,profile=1]
    switchProfileContents(profile.p, &s, true);
    params.arguments << "leaf.scalarProp:cmdline" << "leaf.listProp:cmdline";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 23: [cmdline=1,prod=0,export=1,nonleaf=1,profile=0]
    switchProfileContents(profile.p, &s, false);
    switchFileContents(nonleafFile, true);
    params.arguments << "leaf.scalarProp:cmdline" << "leaf.listProp:cmdline";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 24: [cmdline=1,prod=0,export=1,nonleaf=1,profile=1]
    switchProfileContents(profile.p, &s, true);
    params.arguments << "leaf.scalarProp:cmdline" << "leaf.listProp:cmdline";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 25: [cmdline=1,prod=1,export=0,nonleaf=0,profile=0]
    switchProfileContents(profile.p, &s, false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, false);
    switchFileContents(productFile, true);
    params.arguments << "leaf.scalarProp:cmdline" << "leaf.listProp:cmdline";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 26: [cmdline=1,prod=1,export=0,nonleaf=0,profile=1]
    switchProfileContents(profile.p, &s, true);
    params.arguments << "leaf.scalarProp:cmdline" << "leaf.listProp:cmdline";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 27: [cmdline=1,prod=1,export=0,nonleaf=1,profile=0]
    switchProfileContents(profile.p, &s, false);
    switchFileContents(nonleafFile, true);
    params.arguments << "leaf.scalarProp:cmdline" << "leaf.listProp:cmdline";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 28: [cmdline=1,prod=1,export=0,nonleaf=1,profile=1]
    switchProfileContents(profile.p, &s, true);
    params.arguments << "leaf.scalarProp:cmdline" << "leaf.listProp:cmdline";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 29: [cmdline=1,prod=1,export=1,nonleaf=0,profile=0]
    switchProfileContents(profile.p, &s, false);
    switchFileContents(nonleafFile, false);
    switchFileContents(depFile, true);
    params.arguments << "leaf.scalarProp:cmdline" << "leaf.listProp:cmdline";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 30: [cmdline=1,prod=1,export=1,nonleaf=0,profile=1]
    switchProfileContents(profile.p, &s, true);
    params.arguments << "leaf.scalarProp:cmdline" << "leaf.listProp:cmdline";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 31: [cmdline=1,prod=1,export=1,nonleaf=1,profile=0]
    switchProfileContents(profile.p, &s, false);
    switchFileContents(nonleafFile, true);
    params.arguments << "leaf.scalarProp:cmdline" << "leaf.listProp:cmdline";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());

    // Case 32: [cmdline=1,prod=1,export=1,nonleaf=1,profile=1]
    switchProfileContents(profile.p, &s, true);
    params.arguments << "leaf.scalarProp:cmdline" << "leaf.listProp:cmdline";
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("scalar prop: cmdline\n")
             && m_qbsStdout.contains("list prop: [\"cmdline\"]\n"),
             m_qbsStdout.constData());
}

void TestBlackbox::qmlDebugging()
{
    QDir::setCurrent(testDataDir + "/qml-debugging");
    QCOMPARE(runQbs(), 0);
    Settings settings((QString()));
    Profile profile(profileName(), &settings);
    if (!profile.value("qbs.toolchain").toStringList().contains("gcc"))
        return;
    QProcess nm;
    nm.start("nm", QStringList(relativeExecutableFilePath("debuggable-app")));
    if (nm.waitForStarted()) { // Let's ignore hosts without nm.
        QVERIFY2(nm.waitForFinished(), qPrintable(nm.errorString()));
        QVERIFY2(nm.exitCode() == 0, nm.readAllStandardError().constData());
        const QByteArray output = nm.readAllStandardOutput();
        QVERIFY2(output.toLower().contains("debugginghelper"), output.constData());
    }
}

void TestBlackbox::productDependenciesByType()
{
    QDir::setCurrent(testDataDir + "/product-dependencies-by-type");
    QCOMPARE(runQbs(), 0);
    QFile appListFile(relativeProductBuildDir("app list") + "/app-list.txt");
    QVERIFY2(appListFile.open(QIODevice::ReadOnly), qPrintable(appListFile.fileName()));
    const QList<QByteArray> appList = appListFile.readAll().trimmed().split('\n');
    QCOMPARE(appList.count(), 3);
    QStringList apps = QStringList()
            << QDir::currentPath() + '/' + relativeExecutableFilePath("app1")
            << QDir::currentPath() + '/' + relativeExecutableFilePath("app2")
            << QDir::currentPath() + '/' + relativeExecutableFilePath("app3");
    foreach (const QByteArray &line, appList) {
        const QString cleanLine = QString::fromLocal8Bit(line.trimmed());
        QVERIFY2(apps.removeOne(cleanLine), qPrintable(cleanLine));
    }
    QVERIFY(apps.isEmpty());
}

void TestBlackbox::properQuoting()
{
    QDir::setCurrent(testDataDir + "/proper quoting");
    QCOMPARE(runQbs(), 0);
    QbsRunParameters params(QLatin1String("run"), QStringList() << "-q" << "-p" << "Hello World");
    params.expectFailure = true; // Because the exit code is non-zero.
    QCOMPARE(runQbs(params), 156);
    const char * const expectedOutput = "whitespaceless\ncontains space\ncontains\ttab\n"
            "backslash\\\nHello World! The magic number is 156.";
    QCOMPARE(unifiedLineEndings(m_qbsStdout).constData(), expectedOutput);
}

void TestBlackbox::radAfterIncompleteBuild_data()
{
    QTest::addColumn<QString>("projectFileName");
    QTest::newRow("Project with Rule") << "project_with_rule.qbs";
    QTest::newRow("Project with Transformer") << "project_with_transformer.qbs";
}

void TestBlackbox::radAfterIncompleteBuild()
{
    QDir::setCurrent(testDataDir + "/rad-after-incomplete-build");
    rmDirR(relativeBuildDir());
    QFETCH(QString, projectFileName);

    // Step 1: Have a directory where a file used to be.
    QbsRunParameters params(QStringList() << "-f" << projectFileName);
    QCOMPARE(runQbs(params), 0);
    QVERIFY(projectFileName.contains("transformer") == m_qbsStderr.contains("deprecated"));
    WAIT_FOR_NEW_TIMESTAMP();
    QFile projectFile(projectFileName);
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    QByteArray content = projectFile.readAll();
    content.replace("oldfile", "oldfile/newfile");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.flush();
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    WAIT_FOR_NEW_TIMESTAMP();
    content.replace("oldfile/newfile", "newfile");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.flush();
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    content.replace("newfile", "oldfile/newfile");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.flush();
    QCOMPARE(runQbs(params), 0);

    // Step 2: Have a file where a directory used to be.
    WAIT_FOR_NEW_TIMESTAMP();
    content.replace("oldfile/newfile", "oldfile");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.flush();
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    WAIT_FOR_NEW_TIMESTAMP();
    content.replace("oldfile", "newfile");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.flush();
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
    WAIT_FOR_NEW_TIMESTAMP();
    content.replace("newfile", "oldfile");
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.flush();
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::subProfileChangeTracking()
{
    QDir::setCurrent(testDataDir + "/subprofile-change-tracking");
    qbs::Settings settings((QString()));
    qbs::Internal::TemporaryProfile subProfile("qbs-autotests-subprofile", &settings);
    subProfile.p.setValue("baseProfile", profileName());
    subProfile.p.setValue("cpp.includePaths", QStringList("/tmp/include1"));
    settings.sync();
    QCOMPARE(runQbs(), 0);

    subProfile.p.setValue("cpp.includePaths", QStringList("/tmp/include2"));
    settings.sync();
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("main1.cpp"));
    QVERIFY(m_qbsStdout.contains("main2.cpp"));
}

void TestBlackbox::successiveChanges()
{
    QDir::setCurrent(testDataDir + "/successive-changes");
    QCOMPARE(runQbs(), 0);

    QbsRunParameters params(QStringList() << "theProduct.type:output,blubb");
    QCOMPARE(runQbs(params), 0);

    params.arguments << "project.version:2";
    QCOMPARE(runQbs(params), 0);
    QFile output(relativeProductBuildDir("theProduct") + "/output.out");
    QVERIFY2(output.open(QIODevice::ReadOnly), qPrintable(output.errorString()));
    const QByteArray version = output.readAll();
    QCOMPARE(version.constData(), "2");
}

void TestBlackbox::installedApp()
{
    QDir::setCurrent(testDataDir + "/installed_artifact");

    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(defaultInstallRoot
            + HostOsInfo::appendExecutableSuffix(QLatin1String("/usr/bin/installedApp"))));

    QCOMPARE(runQbs(QbsRunParameters(QStringList("qbs.installRoot:" + testDataDir
                                                 + "/installed-app"))), 0);
    QVERIFY(regularFileExists(testDataDir
            + HostOsInfo::appendExecutableSuffix("/installed-app/usr/bin/installedApp")));

    QFile addedFile(defaultInstallRoot + QLatin1String("/blubb.txt"));
    QVERIFY(addedFile.open(QIODevice::WriteOnly));
    addedFile.close();
    QVERIFY(addedFile.exists());
    QCOMPARE(runQbs(QbsRunParameters(QStringList("--clean-install-root"))), 0);
    QVERIFY(regularFileExists(defaultInstallRoot
            + HostOsInfo::appendExecutableSuffix(QLatin1String("/usr/bin/installedApp"))));
    QVERIFY(regularFileExists(defaultInstallRoot + QLatin1String("/usr/src/main.cpp")));
    QVERIFY(!addedFile.exists());

    // Check whether changing install parameters on the product causes re-installation.
    QFile projectFile("installed_artifact.qbs");
    QVERIFY(projectFile.open(QIODevice::ReadWrite));
    QByteArray content = projectFile.readAll();
    content.replace("qbs.installPrefix: \"/usr\"", "qbs.installPrefix: '/usr/local'");
    WAIT_FOR_NEW_TIMESTAMP();
    projectFile.resize(0);
    projectFile.write(content);
    QVERIFY(projectFile.flush());
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(defaultInstallRoot
            + HostOsInfo::appendExecutableSuffix(QLatin1String("/usr/local/bin/installedApp"))));
    QVERIFY(regularFileExists(defaultInstallRoot + QLatin1String("/usr/local/src/main.cpp")));

    // Check whether changing install parameters on the artifact causes re-installation.
    content.replace("qbs.installDir: \"bin\"", "qbs.installDir: 'custom'");
    WAIT_FOR_NEW_TIMESTAMP();
    projectFile.resize(0);
    projectFile.write(content);
    QVERIFY(projectFile.flush());
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(defaultInstallRoot
            + HostOsInfo::appendExecutableSuffix(QLatin1String("/usr/local/custom/installedApp"))));

    // Check whether changing install parameters on a source file causes re-installation.
    content.replace("qbs.installDir: \"src\"", "qbs.installDir: 'source'");
    WAIT_FOR_NEW_TIMESTAMP();
    projectFile.resize(0);
    projectFile.write(content);
    projectFile.close();
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(defaultInstallRoot + QLatin1String("/usr/local/source/main.cpp")));

    // Check whether changing install parameters on the command line causes re-installation.
    QbsRunParameters(QStringList("qbs.installRoot:" + relativeBuildDir() + "/blubb"));
    QCOMPARE(runQbs(QbsRunParameters(QStringList("qbs.installRoot:" + relativeBuildDir()
                                                 + "/blubb"))), 0);
    QVERIFY(regularFileExists(relativeBuildDir() + "/blubb/usr/local/source/main.cpp"));

    // Check --no-install
    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(QbsRunParameters(QStringList("--no-install"))), 0);
    QCOMPARE(QDir(defaultInstallRoot).entryList(QDir::NoDotAndDotDot).count(), 0);

    // Check --no-build
    rmDirR(relativeBuildDir());
    QbsRunParameters params("install", QStringList("--no-build"));
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("No build graph"));
}

void TestBlackbox::installDuplicates()
{
    QDir::setCurrent(testDataDir + "/install-duplicates");

    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("Cannot install files"));
}

void TestBlackbox::installedSourceFiles()
{
    QDir::setCurrent(testDataDir + "/installed-source-files");

    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(defaultInstallRoot + QLatin1String("/readme.txt")));
    QVERIFY(regularFileExists(defaultInstallRoot + QLatin1String("/main.cpp")));
}

void TestBlackbox::toolLookup()
{
    QbsRunParameters params(QLatin1String("setup-toolchains"), QStringList("--help"));
    params.useProfile = false;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::topLevelSearchPath()
{
    QDir::setCurrent(testDataDir + "/toplevel-searchpath");

    QbsRunParameters params;
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(m_qbsStderr.contains("MyProduct"), m_qbsStderr.constData());
    params.arguments << ("project.qbsSearchPaths:" + QDir::currentPath() + "/qbs-resources");
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::checkProjectFilePath()
{
    QDir::setCurrent(testDataDir + "/project_filepath_check");
    QbsRunParameters params(QStringList("-f") << "project1.qbs");
    QCOMPARE(runQbs(params), 0);
    QCOMPARE(runQbs(params), 0);

    params.arguments = QStringList("-f") << "project2.qbs";
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("project file"));

    params.arguments = QStringList("-f") << "project2.qbs" << "--force";
    params.expectFailure = false;
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStderr.contains("project file"));
}

class TemporaryDefaultProfileRemover
{
public:
    TemporaryDefaultProfileRemover(Settings *settings)
        : m_settings(settings), m_defaultProfile(settings->defaultProfile())
    {
        m_settings->remove(QLatin1String("defaultProfile"));
    }

    ~TemporaryDefaultProfileRemover()
    {
        if (!m_defaultProfile.isEmpty())
            m_settings->setValue(QLatin1String("defaultProfile"), m_defaultProfile);
    }

private:
    Settings *m_settings;
    const QString m_defaultProfile;
};

void TestBlackbox::missingProfile()
{
    Settings settings((QString()));
    TemporaryDefaultProfileRemover dpr(&settings);
    settings.sync();
    QVERIFY(settings.defaultProfile().isEmpty());
    QDir::setCurrent(testDataDir + "/project_filepath_check");
    QbsRunParameters params;
    params.arguments = QStringList("-f") << "project1.qbs";
    params.expectFailure = true;
    params.useProfile = false;
    QVERIFY(runQbs(params) != 0);
    QVERIFY(m_qbsStderr.contains("No profile"));
}

void TestBlackbox::assembly()
{
    Settings settings((QString()));
    Profile profile(profileName(), &settings);
    bool haveGcc = profile.value("qbs.toolchain").toStringList().contains("gcc");
    bool haveMSVC = profile.value("qbs.toolchain").toStringList().contains("msvc");
    QDir::setCurrent(testDataDir + "/assembly");
    QVERIFY(runQbs() == 0);
    QCOMPARE(m_qbsStdout.contains("assembling testa.s"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("compiling testb.S"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("compiling testc.sx"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("creating libtesta.a"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("creating libtestb.a"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("creating libtestc.a"), haveGcc);
    QCOMPARE(m_qbsStdout.contains("creating testd.lib"), haveMSVC);
}

void TestBlackbox::nsis()
{
    QStringList regKeys;
    regKeys << QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\NSIS")
            << QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\NSIS");

    QStringList paths = QProcessEnvironment::systemEnvironment().value("PATH")
            .split(HostOsInfo::pathListSeparator(), QString::SkipEmptyParts);

    foreach (const QString &key, regKeys) {
        QSettings settings(key, QSettings::NativeFormat);
        QString str = settings.value(QLatin1String(".")).toString();
        if (!str.isEmpty())
            paths.prepend(str);
    }

    bool haveMakeNsis = false;
    foreach (const QString &path, paths) {
        if (regularFileExists(QDir::fromNativeSeparators(path) +
                          HostOsInfo::appendExecutableSuffix(QLatin1String("/makensis")))) {
            haveMakeNsis = true;
            break;
        }
    }

    if (!haveMakeNsis) {
        QSKIP("makensis is not installed");
        return;
    }

    bool targetIsWindows = targetOs() == HostOsInfo::HostOsWindows;
    QDir::setCurrent(testDataDir + "/nsis");
    QVERIFY(runQbs() == 0);
    QCOMPARE((bool)m_qbsStdout.contains("compiling hello.nsi"), targetIsWindows);
    QCOMPARE((bool)m_qbsStdout.contains("SetCompressor ignored due to previous call with the /FINAL switch"), targetIsWindows);
    QVERIFY(!QFile::exists(defaultInstallRoot + "/you-should-not-see-a-file-with-this-name.exe"));
}

QString getEmbeddedBinaryPlist(const QString &file)
{
    QProcess p;
    p.start("otool", QStringList() << "-v" << "-X" << "-s" << "__TEXT" << "__info_plist" << file);
    p.waitForFinished();
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

void TestBlackbox::embedInfoPlist()
{
    if (!HostOsInfo::isMacosHost())
        QSKIP("only applies on macOS");

    QDir::setCurrent(testDataDir + QLatin1String("/embedInfoPlist"));

    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);

    QVERIFY(!getEmbeddedBinaryPlist(defaultInstallRoot + "/app").isEmpty());
    QVERIFY(!getEmbeddedBinaryPlist(defaultInstallRoot + "/liblib.dylib").isEmpty());
    QVERIFY(!getEmbeddedBinaryPlist(defaultInstallRoot + "/mod.bundle").isEmpty());

    params.arguments = QStringList(QLatin1String("bundle.embedInfoPlist:false"));
    params.expectFailure = true;
    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(params), 0);

    QVERIFY(getEmbeddedBinaryPlist(defaultInstallRoot + "/app").isEmpty());
    QVERIFY(getEmbeddedBinaryPlist(defaultInstallRoot + "/liblib.dylib").isEmpty());
    QVERIFY(getEmbeddedBinaryPlist(defaultInstallRoot + "/mod.bundle").isEmpty());
}

void TestBlackbox::enableExceptions()
{
    QFETCH(QString, file);
    QFETCH(bool, enable);
    QFETCH(bool, expectSuccess);

    QDir::setCurrent(testDataDir + QStringLiteral("/enableExceptions"));

    QbsRunParameters params;
    params.arguments = QStringList() << "-f" << file << (QStringLiteral("cpp.enableExceptions:")
                                                         + (enable ? "true" : "false"));
    params.expectFailure = !expectSuccess;
    rmDirR(relativeBuildDir());
    if (!params.expectFailure)
        QCOMPARE(runQbs(params), 0);
    else
        QVERIFY(runQbs(params) != 0);
}

void TestBlackbox::enableExceptions_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<bool>("enable");
    QTest::addColumn<bool>("expectSuccess");

    QTest::newRow("no exceptions, enabled") << "none.qbs" << true << true;
    QTest::newRow("no exceptions, disabled") << "none.qbs" << false << true;

    QTest::newRow("C++ exceptions, enabled") << "exceptions.qbs" << true << true;
    QTest::newRow("C++ exceptions, disabled") << "exceptions.qbs" << false << false;

    if (HostOsInfo::isMacosHost()) {
        QTest::newRow("Objective-C exceptions, enabled") << "exceptions-objc.qbs" << true << true;
        QTest::newRow("Objective-C exceptions in Objective-C++ source, enabled") << "exceptions-objcpp.qbs" << true << true;
        QTest::newRow("C++ exceptions in Objective-C++ source, enabled") << "exceptions-objcpp-cpp.qbs" << true << true;
        QTest::newRow("Objective-C, disabled") << "exceptions-objc.qbs" << false << false;
        QTest::newRow("Objective-C exceptions in Objective-C++ source, disabled") << "exceptions-objcpp.qbs" << false << false;
        QTest::newRow("C++ exceptions in Objective-C++ source, disabled") << "exceptions-objcpp-cpp.qbs" << false << false;
    }
}

void TestBlackbox::enableRtti()
{
    QDir::setCurrent(testDataDir + QStringLiteral("/enableRtti"));

    QbsRunParameters params;

    params.arguments = QStringList() << "cpp.enableRtti:true";
    rmDirR(relativeBuildDir());
    QCOMPARE(runQbs(params), 0);

    if (HostOsInfo::isMacosHost()) {
        params.arguments = QStringList() << "cpp.enableRtti:true" << "project.treatAsObjcpp:true";
        rmDirR(relativeBuildDir());
        QCOMPARE(runQbs(params), 0);
    }

    params.expectFailure = true;

    params.arguments = QStringList() << "cpp.enableRtti:false";
    rmDirR(relativeBuildDir());
    QVERIFY(runQbs(params) != 0);

    if (HostOsInfo::isMacosHost()) {
        params.arguments = QStringList() << "cpp.enableRtti:false" << "project.treatAsObjcpp:true";
        rmDirR(relativeBuildDir());
        QVERIFY(runQbs(params) != 0);
    }
}

void TestBlackbox::frameworkStructure()
{
    if (!HostOsInfo::isMacosHost())
        QSKIP("only applies on macOS");

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

    params.arguments = QStringList() << "project.includeHeaders:false";
    QCOMPARE(runQbs(params), 0);

    QVERIFY(!directoryExists(relativeProductBuildDir("Widget") + "/Widget.framework/Headers"));
    QVERIFY(!directoryExists(relativeProductBuildDir("Widget") + "/Widget.framework/PrivateHeaders"));
}

static bool haveWiX(const Profile &profile)
{
    if (profile.value("wix.toolchainInstallPath").isValid() &&
            profile.value("wix.toolchainInstallRoot").isValid()) {
        return true;
    }

    QStringList regKeys;
    regKeys << QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows Installer XML")
            << QLatin1String("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Installer XML");

    QStringList paths = QProcessEnvironment::systemEnvironment().value("PATH")
            .split(HostOsInfo::pathListSeparator(), QString::SkipEmptyParts);

    foreach (const QString &key, regKeys) {
        const QStringList versions = QSettings(key, QSettings::NativeFormat).childGroups();
        foreach (const QString &version, versions) {
            QSettings settings(key + version, QSettings::NativeFormat);
            QString str = settings.value(QLatin1String("InstallRoot")).toString();
            if (!str.isEmpty())
                paths.prepend(str);
        }
    }

    foreach (const QString &path, paths) {
        if (regularFileExists(QDir::fromNativeSeparators(path) +
                          HostOsInfo::appendExecutableSuffix(QLatin1String("/candle"))) &&
            regularFileExists(QDir::fromNativeSeparators(path) +
                          HostOsInfo::appendExecutableSuffix(QLatin1String("/light")))) {
            return true;
        }
    }

    return false;
}

void TestBlackbox::wix()
{
    Settings settings((QString()));
    Profile profile(profileName(), &settings);

    if (!haveWiX(profile)) {
        QSKIP("WiX is not installed");
        return;
    }

    const QByteArray arch = profile.value("qbs.architecture").toString().toLatin1();

    QDir::setCurrent(testDataDir + "/wix");
    QbsRunParameters params;
    if (!HostOsInfo::isWindowsHost())
        params.arguments << "qbs.targetOS:windows";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling QbsSetup.wxs"));
    QVERIFY(m_qbsStdout.contains("linking qbs-" + arch + ".msi"));
    QVERIFY(regularFileExists(relativeProductBuildDir("QbsSetup") + "/qbs-" + arch + ".msi"));

    if (HostOsInfo::isWindowsHost()) {
        QVERIFY(m_qbsStdout.contains("compiling QbsBootstrapper.wxs"));
        QVERIFY(m_qbsStdout.contains("linking qbs-setup-" + arch + ".exe"));
        QVERIFY(regularFileExists(relativeProductBuildDir("QbsBootstrapper")
                                  + "/qbs-setup-" + arch + ".exe"));
    }
}

void TestBlackbox::nodejs()
{
    Settings settings((QString()));
    Profile p(profileName(), &settings);

    int status;
    findNodejs(&status);
    QCOMPARE(status, 0);

    QDir::setCurrent(testDataDir + QLatin1String("/nodejs"));

    status = runQbs();
    if (p.value("nodejs.toolchainInstallPath").toString().isEmpty()
            && status != 0 && m_qbsStderr.contains("toolchainInstallPath")) {
        QSKIP("nodejs.toolchainInstallPath not set and automatic detection failed");
    }

    if (p.value("nodejs.packageManagerPrefixPath").toString().isEmpty()
            && status != 0 && m_qbsStderr.contains("nodejs.packageManagerPrefixPath")) {
        QSKIP("nodejs.packageManagerFilePath not set and automatic detection failed");
    }

    QCOMPARE(status, 0);

    QbsRunParameters params;
    params.command = QLatin1String("run");
    QCOMPARE(runQbs(params), 0);
    QVERIFY((bool)m_qbsStdout.contains("hello world"));
    QVERIFY(regularFileExists(relativeProductBuildDir("hello") + "/hello.js"));
}

void TestBlackbox::typescript()
{
    Settings settings((QString()));
    Profile p(profileName(), &settings);

    int status;
    findTypeScript(&status);
    QCOMPARE(status, 0);

    QDir::setCurrent(testDataDir + QLatin1String("/typescript"));

    status = runQbs();
    if (p.value("typescript.toolchainInstallPath").toString().isEmpty() && status != 0) {
        if (m_qbsStderr.contains("typescript.toolchainInstallPath"))
            QSKIP("typescript.toolchainInstallPath not set and automatic detection failed");
        if (m_qbsStderr.contains("nodejs.interpreterFilePath"))
            QSKIP("nodejs.interpreterFilePath not set and automatic detection failed");
    }

    QCOMPARE(status, 0);

    QbsRunParameters params;
    params.command = QLatin1String("run");
    params.arguments = QStringList() << "-p" << "animals";
    QCOMPARE(runQbs(params), 0);

    QVERIFY(regularFileExists(relativeProductBuildDir("animals") + "/animals.js"));
    QVERIFY(regularFileExists(relativeProductBuildDir("animals") + "/extra.js"));
    QVERIFY(regularFileExists(relativeProductBuildDir("animals") + "/main.js"));
}

void TestBlackbox::iconset()
{
    if (!HostOsInfo::isMacosHost() || !isXcodeProfile(profileName()))
        QSKIP("only applies on macOS with Xcode based profiles");

    QDir::setCurrent(testDataDir + QLatin1String("/ib/iconset"));

    QbsRunParameters params;
    params.arguments = QStringList() << "-f" << "iconset.qbs";
    QCOMPARE(runQbs(params), 0);

    QVERIFY(regularFileExists(relativeProductBuildDir("iconset") + "/white.icns"));
}

void TestBlackbox::iconsetApp()
{
    if (!HostOsInfo::isMacosHost() || !isXcodeProfile(profileName()))
        QSKIP("only applies on macOS with Xcode based profiles");

    QDir::setCurrent(testDataDir + QLatin1String("/ib/iconsetapp"));

    QbsRunParameters params;
    params.arguments = QStringList() << "-f" << "iconsetapp.qbs";
    QCOMPARE(runQbs(params), 0);

    QVERIFY(regularFileExists(relativeProductBuildDir("iconsetapp") + "/iconsetapp.app/Contents/Resources/white.icns"));
}

void TestBlackbox::importingProduct()
{
    QDir::setCurrent(testDataDir + "/importing-product");
    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::infoPlist()
{
    if (!HostOsInfo::isMacosHost())
        QSKIP("only applies on macOS");

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

void TestBlackbox::assetCatalog()
{
    QFETCH(bool, flatten);

    if (!HostOsInfo::isMacosHost() || !isXcodeProfile(profileName()))
        QSKIP("only applies on macOS with Xcode based profiles");

    if (HostOsInfo::hostOsVersion() < qbs::Internal::Version(10, 9))
        QSKIP("This test needs at least macOS 10.9.");

    QDir::setCurrent(testDataDir + QLatin1String("/ib/assetcatalog"));

    rmDirR(relativeBuildDir());

    QbsRunParameters params;
    const QString flattens = "ib.flatten:" + QString(flatten ? "true" : "false");

    // Make sure a dry run does not write anything
    params.arguments = QStringList() << "-f" << "assetcatalogempty.qbs" << "--dry-run" << flattens;
    QCOMPARE(runQbs(params), 0);
    QVERIFY(!directoryExists(relativeBuildDir()));

    params.arguments = QStringList() << "-f" << "assetcatalogempty.qbs" << flattens;
    QCOMPARE(runQbs(params), 0);

    // empty asset catalogs must still produce output
    QVERIFY((bool)m_qbsStdout.contains("compiling empty.xcassets"));

    // should not produce a CAR since minimumMacosVersion will be < 10.9
    QVERIFY(!regularFileExists(relativeProductBuildDir("assetcatalogempty") + "/assetcatalogempty.app/Contents/Resources/Assets.car"));

    rmDirR(relativeBuildDir());
    params.arguments.append("cpp.minimumMacosVersion:10.9"); // force CAR generation
    QCOMPARE(runQbs(params), 0);

    // empty asset catalogs must still produce output
    QVERIFY((bool)m_qbsStdout.contains("compiling empty.xcassets"));
    QVERIFY(regularFileExists(relativeProductBuildDir("assetcatalogempty") + "/assetcatalogempty.app/Contents/Resources/Assets.car"));

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

    QDir::setCurrent(testDataDir + QLatin1String("/ib/multiple-asset-catalogs"));
    rmDirR(relativeBuildDir());
    params.arguments = QStringList();
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(m_qbsStdout.contains("compiling assetcatalog1.xcassets"), m_qbsStdout);
    QVERIFY2(m_qbsStdout.contains("compiling assetcatalog2.xcassets"), m_qbsStdout);

    QDir::setCurrent(testDataDir + QLatin1String("/ib/empty-asset-catalogs"));
    rmDirR(relativeBuildDir());
    params.arguments = QStringList();
    QCOMPARE(runQbs(params), 0);
    QVERIFY2(!m_qbsStdout.contains("compiling assetcatalog1.xcassets"), m_qbsStdout);
    QVERIFY2(!m_qbsStdout.contains("compiling assetcatalog2.xcassets"), m_qbsStdout);
}

void TestBlackbox::assetCatalog_data()
{
    QTest::addColumn<bool>("flatten");
    QTest::newRow("flattened") << true;
    QTest::newRow("unflattened") << false;
}

void TestBlackbox::objcArc()
{
    if (!HostOsInfo::isMacosHost())
        QSKIP("only applies on platforms supporting Objective-C");

    QDir::setCurrent(testDataDir + QLatin1String("/objc-arc"));

    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::outputArtifactAutoTagging()
{
    QDir::setCurrent(testDataDir + QLatin1String("/output-artifact-auto-tagging"));

    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(relativeExecutableFilePath("output-artifact-auto-tagging")));
}

void TestBlackbox::wildCardsAndRules()
{
    QDir::setCurrent(testDataDir + "/wildcards-and-rules");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("Creating output artifact"));
    QFile output(relativeProductBuildDir("wildcards-and-rules") + "/test.mytype");
    QVERIFY2(output.open(QIODevice::ReadOnly), qPrintable(output.errorString()));
    QCOMPARE(output.readAll().count('\n'), 1);
    output.close();

    // Add input.
    touch("input2.inp");
    QbsRunParameters params;
    params.expectFailure = true;
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("Creating output artifact"));
    QVERIFY2(output.open(QIODevice::ReadOnly), qPrintable(output.errorString()));
    QCOMPARE(output.readAll().count('\n'), 2);
    output.close();

    // Add "explicitlyDependsOn".
    touch("dep.dep");
    QCOMPARE(runQbs(), 0);
    QVERIFY(m_qbsStdout.contains("Creating output artifact"));

    // Add nothing.
    QCOMPARE(runQbs(), 0);
    QVERIFY(!m_qbsStdout.contains("Creating output artifact"));
}

void TestBlackbox::loadableModule()
{
    QDir::setCurrent(testDataDir + QLatin1String("/loadablemodule"));

    QCOMPARE(runQbs(), 0);
}

void TestBlackbox::lrelease()
{
    QDir::setCurrent(testDataDir + QLatin1String("/lrelease"));
    QCOMPARE(runQbs(), 0);
    QVERIFY(regularFileExists(relativeProductBuildDir("lrelease-test") + "/de.qm"));
    QVERIFY(regularFileExists(relativeProductBuildDir("lrelease-test") + "/hu.qm"));

    QCOMPARE(runQbs(QString("clean")), 0);
    QbsRunParameters params(QStringList({ "Qt.core.lreleaseMultiplexMode:true"}));
    QCOMPARE(runQbs(params), 0);
    QVERIFY(regularFileExists(relativeProductBuildDir("lrelease-test") + "/lrelease-test.qm"));
    QVERIFY(!regularFileExists(relativeProductBuildDir("lrelease-test") + "/de.qm"));
    QVERIFY(!regularFileExists(relativeProductBuildDir("lrelease-test") + "/hu.qm"));

    QCOMPARE(runQbs(QString("clean")), 0);
    params.arguments << "Qt.core.qmBaseName:somename";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(regularFileExists(relativeProductBuildDir("lrelease-test") + "/somename.qm"));
    QVERIFY(!regularFileExists(relativeProductBuildDir("lrelease-test") + "/lrelease-test.qm"));
    QVERIFY(!regularFileExists(relativeProductBuildDir("lrelease-test") + "/de.qm"));
    QVERIFY(!regularFileExists(relativeProductBuildDir("lrelease-test") + "/hu.qm"));
}

void TestBlackbox::missingDependency()
{
    QDir::setCurrent(testDataDir + "/missing-dependency");
    QbsRunParameters params;
    params.expectFailure = true;
    params.arguments << "-p" << "theApp";
    QVERIFY(runQbs(params) != 0);
    QVERIFY2(!m_qbsStderr.contains("ASSERT"), m_qbsStderr.constData());
    QCOMPARE(runQbs(QbsRunParameters(QStringList() << "-p" << "theDep")), 0);
    params.expectFailure = false;
    params.arguments << "-vv";
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStderr.contains("false positive"));
}

void TestBlackbox::badInterpreter()
{
    if (!HostOsInfo::isAnyUnixHost())
        QSKIP("only applies on Unix");

    QDir::setCurrent(testDataDir + QLatin1String("/badInterpreter"));
    QCOMPARE(runQbs(), 0);

    QbsRunParameters params("run");
    params.expectFailure = true;

    const QRegExp reNoSuchFileOrDir("bad interpreter:.* No such file or directory");
    const QRegExp rePermissionDenied("bad interpreter:.* Permission denied");

    params.arguments = QStringList() << "-p" << "script-interp-missing";
    QCOMPARE(runQbs(params), 1);
    QString strerr = QString::fromLocal8Bit(m_qbsStderr);
    QVERIFY(strerr.contains(reNoSuchFileOrDir));

    params.arguments = QStringList() << "-p" << "script-interp-noexec";
    QCOMPARE(runQbs(params), 1);
    strerr = QString::fromLocal8Bit(m_qbsStderr);
    QVERIFY(strerr.contains(reNoSuchFileOrDir) || strerr.contains(rePermissionDenied));

    params.arguments = QStringList() << "-p" << "script-noexec";
    QCOMPARE(runQbs(params), 1);
    QCOMPARE(runQbs(QbsRunParameters("run", QStringList() << "-p" << "script-ok")), 0);
}

void TestBlackbox::qbsVersion()
{
    const qbs::Internal::Version v = qbs::Internal::Version::qbsVersion();
    QDir::setCurrent(testDataDir + QLatin1String("/qbsVersion"));
    QbsRunParameters params;
    params.arguments = QStringList()
            << "project.qbsVersion:" + v.toString()
            << "project.qbsVersionMajor:" + QString::number(v.majorVersion())
            << "project.qbsVersionMinor:" + QString::number(v.minorVersion())
            << "project.qbsVersionPatch:" + QString::number(v.patchLevel());
    QCOMPARE(runQbs(params), 0);

    params.arguments.append("project.qbsVersionPatch:" + QString::number(v.patchLevel() + 1));
    params.expectFailure = true;
    QVERIFY(runQbs(params) != 0);
}

void TestBlackbox::transitiveOptionalDependencies()
{
    QDir::setCurrent(testDataDir + "/transitive-optional-dependencies");
    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);
}

void TestBlackbox::groupsInModules()
{
    QDir::setCurrent(testDataDir + "/groups-in-modules");
    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compile rock.coal => rock.diamond"));
    QVERIFY(m_qbsStdout.contains("compile chunk.coal => chunk.diamond"));
    QVERIFY(m_qbsStdout.contains("compiling helper2.c"));
    QVERIFY(!m_qbsStdout.contains("compiling helper3.c"));
    QVERIFY(m_qbsStdout.contains("compiling helper4.c"));
    QVERIFY(m_qbsStdout.contains("compiling helper5.c"));
    QVERIFY(!m_qbsStdout.contains("compiling helper6.c"));

    QCOMPARE(runQbs(params), 0);
    QVERIFY(!m_qbsStdout.contains("compile rock.coal => rock.diamond"));
    QVERIFY(!m_qbsStdout.contains("compile chunk.coal => chunk.diamond"));

    WAIT_FOR_NEW_TIMESTAMP();
    touch("modules/helper/diamondc.c");

    QCOMPARE(runQbs(params), 0);
    QVERIFY(m_qbsStdout.contains("compiling diamondc.c"));
    QVERIFY(m_qbsStdout.contains("compile rock.coal => rock.diamond"));
    QVERIFY(m_qbsStdout.contains("compile chunk.coal => chunk.diamond"));
    QVERIFY(regularFileExists(relativeProductBuildDir("groups-in-modules") + "/rock.diamond"));
    QFile output(relativeProductBuildDir("groups-in-modules") + "/rock.diamond");
    QVERIFY(output.open(QIODevice::ReadOnly));
    QCOMPARE(output.readAll().trimmed(), QByteArray("diamond"));
}

void TestBlackbox::probesInNestedModules()
{
    QDir::setCurrent(testDataDir + "/probes-in-nested-modules");
    QbsRunParameters params;
    QCOMPARE(runQbs(params), 0);

    QCOMPARE(m_qbsStdout.count("running probe a"), 1);
    QCOMPARE(m_qbsStdout.count("running probe b"), 1);
    QCOMPARE(m_qbsStdout.count("running probe c"), 1);
    QCOMPARE(m_qbsStdout.count("running second probe a"), 1);

    QVERIFY(m_qbsStdout.contains("product a, outer.somethingElse = goodbye"));
    QVERIFY(m_qbsStdout.contains("product b, inner.something = hahaha"));
    QVERIFY(m_qbsStdout.contains("product c, inner.something = hello"));

    QVERIFY(m_qbsStdout.contains("product a, inner.something = hahaha"));
    QVERIFY(m_qbsStdout.contains("product a, outer.something = hahaha"));
}

void TestBlackbox::xcode()
{
    if (!HostOsInfo::isMacosHost() || !isXcodeProfile(profileName()))
        QSKIP("only applies on macOS with Xcode based profiles");

    QProcess xcodeSelect;
    xcodeSelect.start("xcode-select", QStringList() << "--print-path");
    QVERIFY2(xcodeSelect.waitForStarted(), qPrintable(xcodeSelect.errorString()));
    QVERIFY2(xcodeSelect.waitForFinished(), qPrintable(xcodeSelect.errorString()));
    QVERIFY2(xcodeSelect.exitCode() == 0, qPrintable(xcodeSelect.readAllStandardError().constData()));
    const QString developerPath(QString::fromLocal8Bit(xcodeSelect.readAllStandardOutput().trimmed()));

    QMultiMap<QString, QString> sdks;

    QProcess xcodebuildShowSdks;
    xcodebuildShowSdks.start("xcrun", QStringList() << "xcodebuild" << "-showsdks");
    QVERIFY2(xcodebuildShowSdks.waitForStarted(), qPrintable(xcodebuildShowSdks.errorString()));
    QVERIFY2(xcodebuildShowSdks.waitForFinished(), qPrintable(xcodebuildShowSdks.errorString()));
    QVERIFY2(xcodebuildShowSdks.exitCode() == 0, qPrintable(xcodebuildShowSdks.readAllStandardError().constData()));
    for (const QString &line : QString::fromLocal8Bit(xcodebuildShowSdks.readAllStandardOutput().trimmed()).split('\n', QString::SkipEmptyParts)) {
        static const QRegularExpression regexp(QStringLiteral("\\s+\\-sdk\\s+(?<platform>[a-z]+)(?<version>[0-9]+\\.[0-9]+)$"));
        QRegularExpressionMatch match = regexp.match(line);
        if (match.isValid()) {
            sdks.insert(match.captured("platform"), match.captured("version"));
        }
    }

    // values() returns items with most recently added first; we want the reverse of that
    QStringList sdkValues(sdks.values("macosx"));
    std::reverse(std::begin(sdkValues), std::end(sdkValues));

    QDir::setCurrent(testDataDir + "/xcode");
    QbsRunParameters params;
    params.arguments = (QStringList()
                        << (QStringLiteral("xcode.developerPath:") + developerPath)
                        << (QStringLiteral("project.sdks:") + sdkValues.join(",")));
    QCOMPARE(runQbs(params), 0);
}

QTEST_MAIN(TestBlackbox)
