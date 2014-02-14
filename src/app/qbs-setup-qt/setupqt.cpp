/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
#include <qtprofilesetup.h>
#include <logging/translator.h>
#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/settings.h>

#include <QByteArrayMatcher>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegExp>
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
    qmakeProcess.start(qmakePath, QStringList() << QLatin1String("-query"));
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
        success = regexp.exactMatch(QString::fromLocal8Bit(configContentLine));
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
    const QStringList parts = versionString.split(QLatin1Char('.'), QString::SkipEmptyParts);
    const QList<int *> vparts = QList<int *>() << &v.majorVersion << &v.minorVersion << &v.patchLevel;
    const int c = qMin(parts.count(), vparts.count());
    for (int i = 0; i < c; ++i)
        *vparts[i] = parts.at(i).toInt();
    return v;
}

typedef QMap<QByteArray, QByteArray> QueryMap;

static QString pathQueryValue(const QueryMap &queryMap, const QByteArray &key)
{
    return QDir::fromNativeSeparators(QString::fromLocal8Bit(queryMap.value(key)));
}

QtEnvironment SetupQt::fetchEnvironment(const QString &qmakePath)
{
    QtEnvironment qtEnvironment;
    QueryMap queryOutput = qmakeQueryOutput(qmakePath);

    qtEnvironment.installPrefixPath = pathQueryValue(queryOutput, "QT_INSTALL_PREFIX");
    qtEnvironment.documentationPath = pathQueryValue(queryOutput, "QT_INSTALL_DOCS");
    qtEnvironment.includePath = pathQueryValue(queryOutput, "QT_INSTALL_HEADERS");
    qtEnvironment.libraryPath = pathQueryValue(queryOutput, "QT_INSTALL_LIBS");
    qtEnvironment.binaryPath = pathQueryValue(queryOutput, "QT_INSTALL_BINS");
    qtEnvironment.documentationPath = pathQueryValue(queryOutput, "QT_INSTALL_DOCS");
    qtEnvironment.pluginPath = pathQueryValue(queryOutput, "QT_INSTALL_PLUGINS");
    qtEnvironment.qmlImportPath = pathQueryValue(queryOutput, "QT_INSTALL_IMPORTS");
    qtEnvironment.qtVersion = QString::fromLocal8Bit(queryOutput.value("QT_VERSION"));

    const Version qtVersion = extractVersion(qtEnvironment.qtVersion);

    QString mkspecsBaseSrcPath;
    if (qtVersion.majorVersion >= 5) {
        qtEnvironment.mkspecBasePath
                = pathQueryValue(queryOutput, "QT_HOST_DATA") + QLatin1String("/mkspecs");
        mkspecsBaseSrcPath
                = pathQueryValue(queryOutput, "QT_HOST_DATA/src") + QLatin1String("/mkspecs");
    } else {
        qtEnvironment.mkspecBasePath
                = pathQueryValue(queryOutput, "QT_INSTALL_DATA") + QLatin1String("/mkspecs");
    }

    if (!QFile::exists(qtEnvironment.mkspecBasePath))
        throw ErrorInfo(tr("Cannot extract the mkspecs directory."));

    const QByteArray qconfigContent = readFileContent(qtEnvironment.mkspecBasePath
                                                      + QLatin1String("/qconfig.pri"));
    qtEnvironment.qtMajorVersion = configVariable(qconfigContent,
                                                  QLatin1String("QT_MAJOR_VERSION")).toInt();
    qtEnvironment.qtMinorVersion = configVariable(qconfigContent,
                                                  QLatin1String("QT_MINOR_VERSION")).toInt();
    qtEnvironment.qtPatchVersion = configVariable(qconfigContent,
                                                  QLatin1String("QT_PATCH_VERSION")).toInt();
    qtEnvironment.qtNameSpace = configVariable(qconfigContent, QLatin1String("QT_NAMESPACE"));
    qtEnvironment.qtLibInfix = configVariable(qconfigContent, QLatin1String("QT_LIBINFIX"));
    qtEnvironment.architecture = configVariable(qconfigContent, QLatin1String("QT_TARGET_ARCH"));
    if (qtEnvironment.architecture.isEmpty())
        qtEnvironment.architecture = configVariable(qconfigContent, QLatin1String("QT_ARCH"));
    if (qtEnvironment.architecture.isEmpty())
        qtEnvironment.architecture = QLatin1String("x86");
    qtEnvironment.configItems = configVariableItems(qconfigContent, QLatin1String("CONFIG"));
    qtEnvironment.qtConfigItems = configVariableItems(qconfigContent, QLatin1String("QT_CONFIG"));

    // retrieve the mkspec
    if (qtVersion.majorVersion >= 5) {
        const QString mkspecName = QString::fromLocal8Bit(queryOutput.value("QMAKE_XSPEC"));
        qtEnvironment.mkspecName = mkspecName;
        qtEnvironment.mkspecPath = qtEnvironment.mkspecBasePath + QLatin1Char('/') + mkspecName;
        if (!mkspecsBaseSrcPath.isEmpty() && !QFile::exists(qtEnvironment.mkspecPath))
            qtEnvironment.mkspecPath = mkspecsBaseSrcPath + QLatin1Char('/') + mkspecName;
    } else {
        if (HostOsInfo::isWindowsHost()) {
            const QByteArray fileContent = readFileContent(qtEnvironment.mkspecBasePath
                                                           + QLatin1String("/default/qmake.conf"));
            qtEnvironment.mkspecPath = configVariable(fileContent,
                                                      QLatin1String("QMAKESPEC_ORIGINAL"));
        } else {
            qtEnvironment.mkspecPath
                    = QFileInfo(qtEnvironment.mkspecBasePath + "/default").symLinkTarget();
        }
        qtEnvironment.mkspecName = qtEnvironment.mkspecPath;
        int idx = qtEnvironment.mkspecName.lastIndexOf(QLatin1Char('/'));
        if (idx != -1)
            qtEnvironment.mkspecName.remove(0, idx + 1);
    }

    // determine whether we have a framework build
    qtEnvironment.frameworkBuild = false;
    if (qtEnvironment.mkspecPath.contains(QLatin1String("macx"))) {
        if (qtEnvironment.configItems.contains(QLatin1String("qt_framework")))
            qtEnvironment.frameworkBuild = true;
        else if (!qtEnvironment.configItems.contains(QLatin1String("qt_no_framework")))
            throw ErrorInfo(tr("could not determine whether Qt is a frameworks build"));
    }

    // determine whether Qt is built with debug, release or both
    if (qtEnvironment.qtConfigItems.contains(QLatin1String("debug_and_release"))) {
        qtEnvironment.buildVariant << QLatin1String("debug") << QLatin1String("release");
    } else {
        int idxDebug = qtEnvironment.qtConfigItems.indexOf(QLatin1String("debug"));
        int idxRelease = qtEnvironment.qtConfigItems.indexOf(QLatin1String("release"));
        if (idxDebug < idxRelease)
            qtEnvironment.buildVariant << QLatin1String("release");
        else
            qtEnvironment.buildVariant << QLatin1String("debug");
    }

    if (!QFileInfo(qtEnvironment.mkspecPath).exists())
        throw ErrorInfo(tr("mkspec '%1' does not exist").arg(qtEnvironment.mkspecPath));

    return qtEnvironment;
}

void SetupQt::saveToQbsSettings(const QString &qtVersionName, const QtEnvironment &qtEnvironment,
                                Settings *settings)
{
    const QString cleanQtVersionName = Profile::cleanName(qtVersionName);
    QString msg = QCoreApplication::translate("SetupQt", "Creating profile '%0'.")
            .arg(cleanQtVersionName);
    printf("%s\n", qPrintable(msg));

    const ErrorInfo errorInfo = setupQtProfile(cleanQtVersionName, settings, qtEnvironment);
    if (errorInfo.hasError())
        throw errorInfo;

    // If this profile does not specify a toolchain and we find exactly one profile that looks
    // like it might have been added by qbs-setup-toolchains, let's use that one as our
    // base profile.
    Profile profile(cleanQtVersionName, settings);
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
                              "using qbs-setup-toolchains and set it as this profile's "
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
