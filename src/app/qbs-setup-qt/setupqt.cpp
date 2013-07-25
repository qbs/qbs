/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "setupqt.h"

#include "../shared/logging/consolelogger.h"
#include <logging/translator.h>
#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/settings.h>

#include <QByteArrayMatcher>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QProcess>
#include <QStringList>
#include <QtDebug>

namespace qbs {
using Internal::HostOsInfo;
using Internal::Tr;

const QString qmakeExecutableName = QLatin1String("qmake" QTC_HOST_EXE_SUFFIX);

struct Version
{
    Version()
        : majorVersion(0), minorVersion(0), patchLevel(0)
    {}

    int majorVersion;
    int minorVersion;
    int patchLevel;
};

static QStringList collectQmakePaths()
{
    QStringList qmakePaths;

    QByteArray environmentPath = qgetenv("PATH");
    QList<QByteArray> environmentPaths
            = environmentPath.split(HostOsInfo::pathListSeparator().toLatin1());
    foreach (const QByteArray &path, environmentPaths) {
        QFileInfo pathFileInfo(QDir(QLatin1String(path)), qmakeExecutableName);
        if (pathFileInfo.exists()) {
            QString qmakePath = pathFileInfo.absoluteFilePath();
            if (!qmakePaths.contains(qmakePath))
                qmakePaths.append(qmakePath);
        }
    }

    return qmakePaths;
}

bool SetupQt::isQMakePathValid(const QString &qmakePath)
{
    QFileInfo qmakeFileInfo(qmakePath);
    return qmakeFileInfo.exists() && qmakeFileInfo.isFile() && qmakeFileInfo.isExecutable();
}

QList<QtEnvironment> SetupQt::fetchEnvironments()
{
    QList<QtEnvironment> qtEnvironments;

    foreach (const QString &qmakePath, collectQmakePaths())
        qtEnvironments.append(fetchEnvironment(qmakePath));

    return qtEnvironments;
}

static QMap<QByteArray, QByteArray> qmakeQueryOutput(const QString &qmakePath)
{
    QProcess qmakeProcess;
    qmakeProcess.start(qmakePath, QStringList() << "-query");
    if (!qmakeProcess.waitForStarted())
        throw ErrorInfo(SetupQt::tr("%1 cannot be started.").arg(qmakePath));
    qmakeProcess.waitForFinished();
    const QByteArray output = qmakeProcess.readAllStandardOutput();

    QMap<QByteArray, QByteArray> ret;
    foreach (const QByteArray &line, output.split('\n')) {
        int idx = line.indexOf(':');
        if (idx >= 0)
            ret[line.left(idx)] = line.mid(idx + 1).trimmed();
    }
    return ret;
}

static QByteArray readFileContent(const QString &filePath)
{
    QFile file(filePath);
    if (file.open(QFile::ReadOnly))
        return file.readAll();

    return QByteArray();
}

static QString configVariable(const QByteArray &configContent, const QString &key)
{
    QRegExp regexp(QLatin1String("\\s*") + key + QLatin1String("\\s*\\+{0,1}=(.*)"),
                   Qt::CaseSensitive);

    QList<QByteArray> configContentLines = configContent.split('\n');

    bool success = false;

    foreach (const QByteArray &configContentLine, configContentLines) {
        success = regexp.exactMatch(configContentLine);
        if (success)
            break;
    }

    if (success)
        return regexp.capturedTexts()[1].simplified();

    return QString();
}

static QStringList configVariableItems(const QByteArray &configContent, const QString &key)
{
    return configVariable(configContent, key).split(QLatin1Char(' '), QString::SkipEmptyParts);
}

static Version extractVersion(const QString &versionString)
{
    Version v;
    const QStringList parts = versionString.split('.', QString::SkipEmptyParts);
    const QList<int *> vparts = QList<int *>() << &v.majorVersion << &v.minorVersion << &v.patchLevel;
    const int c = qMin(parts.count(), vparts.count());
    for (int i = 0; i < c; ++i)
        *vparts[i] = parts.at(i).toInt();
    return v;
}

QtEnvironment SetupQt::fetchEnvironment(const QString &qmakePath)
{
    QtEnvironment qtEnvironment;
    QMap<QByteArray, QByteArray> queryOutput = qmakeQueryOutput(qmakePath);

    qtEnvironment.installPrefixPath = queryOutput.value("QT_INSTALL_PREFIX");
    qtEnvironment.documentationPath = queryOutput.value("QT_INSTALL_DOCS");
    qtEnvironment.includePath = queryOutput.value("QT_INSTALL_HEADERS");
    qtEnvironment.libraryPath = queryOutput.value("QT_INSTALL_LIBS");
    qtEnvironment.binaryPath = queryOutput.value("QT_INSTALL_BINS");
    qtEnvironment.documentationPath = queryOutput.value("QT_INSTALL_DOCS");
    qtEnvironment.pluginPath = queryOutput.value("QT_INSTALL_PLUGINS");
    qtEnvironment.qmlImportPath = queryOutput.value("QT_INSTALL_IMPORTS");
    qtEnvironment.qtVersion = queryOutput.value("QT_VERSION");

    const Version qtVersion = extractVersion(qtEnvironment.qtVersion);

    QByteArray mkspecsBasePath;
    if (qtVersion.majorVersion >= 5)
        mkspecsBasePath = queryOutput.value("QT_HOST_DATA") + "/mkspecs";
    else
        mkspecsBasePath = queryOutput.value("QT_INSTALL_DATA") + "/mkspecs";

    if (!QFile::exists(mkspecsBasePath))
        throw ErrorInfo(tr("Cannot extract the mkspecs directory."));

    const QByteArray qconfigContent = readFileContent(mkspecsBasePath + "/qconfig.pri");
    qtEnvironment.qtMajorVersion = configVariable(qconfigContent, "QT_MAJOR_VERSION").toInt();
    qtEnvironment.qtMinorVersion = configVariable(qconfigContent, "QT_MINOR_VERSION").toInt();
    qtEnvironment.qtPatchVersion = configVariable(qconfigContent, "QT_PATCH_VERSION").toInt();
    qtEnvironment.qtNameSpace = configVariable(qconfigContent, "QT_NAMESPACE");
    qtEnvironment.qtLibInfix = configVariable(qconfigContent, "QT_LIBINFIX");
    qtEnvironment.configItems = configVariableItems(qconfigContent, QLatin1String("CONFIG"));
    qtEnvironment.qtConfigItems = configVariableItems(qconfigContent, QLatin1String("QT_CONFIG"));

    // retrieve the mkspec
    if (qtVersion.majorVersion >= 5) {
        const QString mkspecName = queryOutput.value("QMAKE_XSPEC");
        qtEnvironment.mkspecPath = mkspecsBasePath + QLatin1Char('/') + mkspecName;
    } else {
        if (HostOsInfo::isWindowsHost()) {
            const QByteArray fileContent = readFileContent(mkspecsBasePath + "/default/qmake.conf");
            qtEnvironment.mkspecPath = configVariable(fileContent, "QMAKESPEC_ORIGINAL");
        } else {
            qtEnvironment.mkspecPath = QFileInfo(mkspecsBasePath + "/default").symLinkTarget();
        }
    }

    // determine whether we have a framework build
    qtEnvironment.frameworkBuild = false;
    if (qtEnvironment.mkspecPath.contains("macx")) {
        if (qtEnvironment.configItems.contains("qt_framework"))
            qtEnvironment.frameworkBuild = true;
        else if (!qtEnvironment.configItems.contains("qt_no_framework"))
            throw ErrorInfo(tr("could not determine whether Qt is a frameworks build"));
    }

    // determine whether we have a static build
    if (qtVersion.majorVersion >= 5) {
        qtEnvironment.staticBuild = qtEnvironment.qtConfigItems.contains(QLatin1String("static"));
    } else {
        if (qtEnvironment.frameworkBuild) {
            // there are no Qt4 static frameworks
            qtEnvironment.staticBuild = false;
        } else {
            qtEnvironment.staticBuild = true;
            QDir libdir(qtEnvironment.libraryPath);
            const QStringList coreLibFiles
                    = libdir.entryList(QStringList(QLatin1String("*Core*")), QDir::Files);
            if (coreLibFiles.isEmpty())
                throw ErrorInfo(tr("Could not determine whether Qt is a static build."));
            foreach (const QString &fileName, coreLibFiles) {
                if (QLibrary::isLibrary(qtEnvironment.libraryPath + QLatin1Char('/') + fileName)) {
                    qtEnvironment.staticBuild = false;
                    break;
                }
            }
        }
    }

    // determine whether Qt is built with debug, release or both
    if (qtEnvironment.qtConfigItems.contains("debug_and_release")) {
        qtEnvironment.buildVariant << QLatin1String("debug") << QLatin1String("release");
    } else {
        int idxDebug = qtEnvironment.qtConfigItems.indexOf("debug");
        int idxRelease = qtEnvironment.qtConfigItems.indexOf("release");
        if (idxDebug < idxRelease)
            qtEnvironment.buildVariant << QLatin1String("release");
        else
            qtEnvironment.buildVariant << QLatin1String("debug");
    }

    if (!QFileInfo(qtEnvironment.mkspecPath).exists())
        throw ErrorInfo(tr("mkspec '%1' does not exist").arg(qtEnvironment.mkspecPath));

    qtEnvironment.mkspecPath = QDir::toNativeSeparators(qtEnvironment.mkspecPath);
    return qtEnvironment;
}

void SetupQt::saveToQbsSettings(const QString &qtVersionName, const QtEnvironment &qtEnvironment,
                                Settings *settings)
{
    const QString cleanQtVersionName = Profile::cleanName(qtVersionName);
    QString msg = QCoreApplication::translate("SetupQt", "Creating profile '%0'.")
            .arg(cleanQtVersionName);
    printf("%s\n", qPrintable(msg));

    Profile profile(cleanQtVersionName, settings);
    const QString settingsTemplate(QLatin1String("Qt.core.%1"));
    profile.setValue(settingsTemplate.arg("qtConfig"), qtEnvironment.qtConfigItems);
    profile.setValue(settingsTemplate.arg("binPath"), qtEnvironment.binaryPath);
    profile.setValue(settingsTemplate.arg("libPath"), qtEnvironment.libraryPath);
    profile.setValue(settingsTemplate.arg("pluginPath"), qtEnvironment.pluginPath);
    profile.setValue(settingsTemplate.arg("incPath"), qtEnvironment.includePath);
    profile.setValue(settingsTemplate.arg("mkspecPath"), qtEnvironment.mkspecPath);
    profile.setValue(settingsTemplate.arg("docPath"), qtEnvironment.documentationPath);
    profile.setValue(settingsTemplate.arg("version"), qtEnvironment.qtVersion);
    profile.setValue(settingsTemplate.arg("namespace"), qtEnvironment.qtNameSpace);
    profile.setValue(settingsTemplate.arg("libInfix"), qtEnvironment.qtLibInfix);
    profile.setValue(settingsTemplate.arg("buildVariant"), qtEnvironment.buildVariant);
    if (qtEnvironment.staticBuild)
        profile.setValue(settingsTemplate.arg(QLatin1String("staticBuild")),
                         qtEnvironment.staticBuild);

    // Set the minimum operating system versions appropriate for this Qt version
    QString windowsVersion, osxVersion, iosVersion, androidVersion;

    // ### TODO: Also dependent on the toolchain, which we can't easily access here
    // Most 32-bit Windows applications based on either Qt 5 or Qt 4 should run
    // on 2000, so this is a good baseline for now.
    if (qtEnvironment.mkspecPath.contains("win32") && qtEnvironment.qtMajorVersion >= 4)
        windowsVersion = QLatin1String("5.0");

    if (qtEnvironment.mkspecPath.contains("macx")) {
        profile.setValue(settingsTemplate.arg("frameworkBuild"), qtEnvironment.frameworkBuild);
        if (qtEnvironment.qtMajorVersion >= 5) {
            osxVersion = QLatin1String("10.6");
        } else if (qtEnvironment.qtMajorVersion == 4 && qtEnvironment.qtMinorVersion >= 6) {
            QDir qconfigDir;
            if (qtEnvironment.frameworkBuild) {
                qconfigDir.setPath(qtEnvironment.libraryPath);
                qconfigDir.cd("QtCore.framework/Headers");
            } else {
                qconfigDir.setPath(qtEnvironment.includePath);
                qconfigDir.cd("Qt");
            }
            QFile qconfig(qconfigDir.absoluteFilePath("qconfig.h"));
            if (qconfig.open(QIODevice::ReadOnly)) {
                bool qtCocoaBuild = false;
                QTextStream ts(&qconfig);
                QString line;
                do {
                    line = ts.readLine();
                    if (QRegExp(QLatin1String("\\s*#define\\s+QT_MAC_USE_COCOA\\s+1\\s*"), Qt::CaseSensitive).exactMatch(line)) {
                        qtCocoaBuild = true;
                        break;
                    }
                } while (!line.isNull());

                if (ts.status() == QTextStream::Ok)
                    osxVersion = qtCocoaBuild ? QLatin1String("10.5") : QLatin1String("10.4");
            }

            if (osxVersion.isEmpty())
                throw ErrorInfo(tr("error reading qconfig.h; could not determine whether Qt is using Cocoa or Carbon"));
        }
    }

    if (qtEnvironment.mkspecPath.contains("ios") && qtEnvironment.qtMajorVersion >= 5)
        iosVersion = QLatin1String("4.3");

    if (qtEnvironment.mkspecPath.contains("android")) {
        if (qtEnvironment.qtMajorVersion >= 5)
            androidVersion = QLatin1String("2.3");
        else if (qtEnvironment.qtMajorVersion == 4 && qtEnvironment.qtMinorVersion >= 8)
            androidVersion = QLatin1String("1.6"); // Necessitas
    }

    if (qtEnvironment.mkspecPath.contains("winrt") && qtEnvironment.qtMajorVersion >= 5)
        windowsVersion = QLatin1String("6.2");

    // ### TODO: wince, winphone, blackberry

    if (!windowsVersion.isEmpty())
        profile.setValue(QLatin1String("cpp.minimumWindowsVersion"), windowsVersion);

    if (!osxVersion.isEmpty())
        profile.setValue(QLatin1String("cpp.minimumOsxVersion"), osxVersion);

    if (!iosVersion.isEmpty())
        profile.setValue(QLatin1String("cpp.minimumIosVersion"), iosVersion);

    if (!androidVersion.isEmpty())
        profile.setValue(QLatin1String("cpp.minimumAndroidVersion"), androidVersion);

    // If this profile does not specify a toolchain and we find exactly one profile that looks
    // like it might have been added by qbs-detect-toolchain, let's use that one as our
    // base profile.
    if (!profile.baseProfile().isEmpty())
        return;
    QStringList toolchainProfiles;
    foreach (const QString &key, profile.allKeys(Profile::KeySelectionNonRecursive)) {
        if (key.startsWith(QLatin1String("cpp.")))
            return;
    }

    foreach (const QString &profileName, settings->profiles()) {
        if (profileName == profile.name())
            continue;
        const Profile otherProfile(profileName, settings);
        bool hasCppKey = false;
        bool hasQtKey = false;
        foreach (const QString &key, otherProfile.allKeys(Profile::KeySelectionNonRecursive)) {
            if (key.startsWith(QLatin1String("cpp."))) {
                hasCppKey = true;
            } else if (key.startsWith(QLatin1String("Qt."))) {
                hasQtKey = true;
                break;
            }
        }
        if (hasCppKey && !hasQtKey)
            toolchainProfiles << profileName;
    }

    if (toolchainProfiles.count() != 1) {
        QString message = Tr::tr("You need to set up toolchain information before you can "
                                 "use this Qt version for building. ");
        if (toolchainProfiles.isEmpty()) {
            message += Tr::tr("However, no toolchain profile was found. Either create one "
                              "using qbs-detect-toolchains and set it as this profile's "
                              "base profile or add the toolchain settings manually "
                              "to this profile.");
        } else {
            message += Tr::tr("Consider setting one of these profiles as this profile's base "
                              "profile: %1.").arg(toolchainProfiles.join(QLatin1String(", ")));
        }
        qbsWarning() << message;
    } else {
        profile.setBaseProfile(toolchainProfiles.first());
        qbsInfo() << Tr::tr("Setting profile '%1' as the base profile for this profile.")
                     .arg(toolchainProfiles.first());
    }
}

bool SetupQt::checkIfMoreThanOneQtWithTheSameVersion(const QString &qtVersion,
        const QList<QtEnvironment> &qtEnvironments)
{
    bool foundOneVersion = false;
    foreach (const QtEnvironment &qtEnvironment, qtEnvironments) {
        if (qtEnvironment.qtVersion == qtVersion) {
            if (foundOneVersion)
                return true;
            foundOneVersion = true;
        }
    }

    return false;
}

} // namespace qbs
