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

    QByteArray enviromentPath = qgetenv("PATH");
    QList<QByteArray> enviromentPaths
            = enviromentPath.split(HostOsInfo::pathListSeparator().toLatin1());
    foreach (const QByteArray &path, enviromentPaths) {
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

QList<QtEnviroment> SetupQt::fetchEnviroments()
{
    QList<QtEnviroment> qtEnviroments;

    foreach (const QString &qmakePath, collectQmakePaths())
        qtEnviroments.append(fetchEnviroment(qmakePath));

    return qtEnviroments;
}

static QMap<QByteArray, QByteArray> qmakeQueryOutput(const QString &qmakePath)
{
    QProcess qmakeProcess;
    qmakeProcess.start(qmakePath, QStringList() << "-query");
    if (!qmakeProcess.waitForStarted())
        throw Error(SetupQt::tr("%1 cannot be started.").arg(qmakePath));
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

static QByteArray configVariable(const QByteArray &configContent, const QByteArray &key)
{
    QRegExp regularExpression(QString(".*%1\\s*\\+{0,1}=(.*)").arg(QString::fromLatin1(key)), Qt::CaseSensitive);

    QList<QByteArray> configContentLines = configContent.split('\n');

    bool success = false;

    foreach (const QByteArray &configContentLine, configContentLines) {
        success = regularExpression.exactMatch(configContentLine);
        if (success)
            break;
    }

    if (success)
        return regularExpression.capturedTexts()[1].simplified().toLatin1();

    return QByteArray();
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

QtEnviroment SetupQt::fetchEnviroment(const QString &qmakePath)
{
    QtEnviroment qtEnvironment;
    QMap<QByteArray, QByteArray> queryOutput = qmakeQueryOutput(qmakePath);

    qtEnvironment.installPrefixPath = queryOutput.value("QT_INSTALL_PREFIX");
    qtEnvironment.documentationPath = queryOutput.value("QT_INSTALL_DOCS");
    qtEnvironment.includePath = queryOutput.value("QT_INSTALL_HEADERS");
    qtEnvironment.libraryPath = queryOutput.value("QT_INSTALL_LIBS");
    qtEnvironment.binaryPath = queryOutput.value("QT_INSTALL_BINS");
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
        throw Error(tr("Cannot extract the mkspecs directory."));

    const QByteArray qconfigContent = readFileContent(mkspecsBasePath + "/qconfig.pri");
    qtEnvironment.qtMajorVersion = configVariable(qconfigContent, "QT_MAJOR_VERSION").toInt();
    qtEnvironment.qtMinorVersion = configVariable(qconfigContent, "QT_MINOR_VERSION").toInt();
    qtEnvironment.qtPatchVersion = configVariable(qconfigContent, "QT_PATCH_VERSION").toInt();
    qtEnvironment.qtNameSpace = configVariable(qconfigContent, "QT_NAMESPACE");
    qtEnvironment.qtLibInfix = configVariable(qconfigContent, "QT_LIBINFIX");
    qtEnvironment.configItems = QString(configVariable(qconfigContent, "CONFIG")).split(QLatin1Char(' '), QString::SkipEmptyParts);

    // read mkspec
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

    if (!QFileInfo(qtEnvironment.mkspecPath).exists())
        throw Error(tr("mkspec '%1' does not exist").arg(qtEnvironment.mkspecPath));

    qtEnvironment.mkspecPath = QDir::toNativeSeparators(qtEnvironment.mkspecPath);
    return qtEnvironment;
}

void SetupQt::saveToQbsSettings(const QString &qtVersionName, const QtEnviroment &qtEnviroment,
                                Settings *settings)
{
    QString msg = QCoreApplication::translate("SetupQt", "Creating profile '%0'.").arg(qtVersionName);
    printf("%s\n", qPrintable(msg));

    Profile profile(qtVersionName, settings);
    const QString settingsTemplate(QLatin1String("qt.core.%1"));
    profile.setValue(settingsTemplate.arg("binPath"), qtEnviroment.binaryPath);
    profile.setValue(settingsTemplate.arg("libPath"), qtEnviroment.libraryPath);
    profile.setValue(settingsTemplate.arg("incPath"), qtEnviroment.includePath);
    profile.setValue(settingsTemplate.arg("mkspecPath"), qtEnviroment.mkspecPath);
    profile.setValue(settingsTemplate.arg("version"), qtEnviroment.qtVersion);
    profile.setValue(settingsTemplate.arg("namespace"), qtEnviroment.qtNameSpace);
    profile.setValue(settingsTemplate.arg("libInfix"), qtEnviroment.qtLibInfix);

    if (qtEnviroment.mkspecPath.contains("macx")) {
        if (qtEnviroment.configItems.contains("qt_framework")) {
            profile.setValue(settingsTemplate.arg("frameworkBuild"), true);
        } else if (qtEnviroment.configItems.contains("qt_no_framework")) {
            profile.setValue(settingsTemplate.arg("frameworkBuild"), false);
        } else {
            throw Error(tr("could not determine whether Qt is a frameworks build"));
        }
    }

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
            } else if (key.startsWith(QLatin1String("qt."))) {
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

bool SetupQt::checkIfMoreThanOneQtWithTheSameVersion(const QString &qtVersion, const QList<QtEnviroment> &qtEnviroments)
{
    bool foundOneVersion = false;
    foreach (const QtEnviroment &qtEnviroment, qtEnviroments) {
        if (qtEnviroment.qtVersion == qtVersion) {
            if (foundOneVersion)
                return true;
            foundOneVersion = true;
        }
    }

    return false;
}

} // namespace qbs
