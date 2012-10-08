/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#include <tools/hostosinfo.h>

#include <QByteArrayMatcher>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QSettings>
#include <QtDebug>

namespace qbs {

const QString qmakeExecutableName = QLatin1String("qmake" QTC_HOST_EXE_SUFFIX);

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
    if (!qmakeFileInfo.exists())
        return false;
    if (qmakeFileInfo.fileName() != qmakeExecutableName)
        return false;
    if (!qmakeFileInfo.isExecutable())
        return false;
    return true;
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

static QByteArray mkSpecContent(const QByteArray &mkSpecPath)
{
    QFile qconfigFile(mkSpecPath + "/qconfig.pri");
    if (qconfigFile.open(QFile::ReadOnly))
        return qconfigFile.readAll();

    return QByteArray();
}

static QByteArray configVariable(const QByteArray &configContent, const QByteArray &key)
{
    QRegExp regularExpression(QString(".*%1\\s*=(.*)").arg(QString::fromLatin1(key)), Qt::CaseSensitive);

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

static QByteArray qmakeConfContent(const QByteArray &mkSpecPath)
{
    QFile qconfigFile(mkSpecPath + "/default/qmake.conf");
    if (qconfigFile.open(QFile::ReadOnly))
        return qconfigFile.readAll();

    return QByteArray();
}

static QString mkSpecPath(const QByteArray &mkspecsPath)
{
    if (HostOsInfo::isWindowsHost())
        return configVariable(qmakeConfContent(mkspecsPath), "QMAKESPEC_ORIGINAL");
    return QFileInfo(mkspecsPath + "/default").symLinkTarget();
}

QtEnviroment SetupQt::fetchEnviroment(const QString &qmakePath)
{
    QtEnviroment qtEnvironment;
    QMap<QByteArray, QByteArray> queryOutput = qmakeQueryOutput(qmakePath);

    qtEnvironment.installPrefixPath = queryOutput.value("QT_INSTALL_PREFIX");
    qtEnvironment.documentationPath = queryOutput.value("QT_INSTALL_DOCS");
    qtEnvironment.includePath = queryOutput.value("QT_INSTALL_HEADERS");
    qtEnvironment.libaryPath = queryOutput.value("QT_INSTALL_LIBS");
    qtEnvironment.binaryPath = queryOutput.value("QT_INSTALL_BINS");
    qtEnvironment.pluginPath = queryOutput.value("QT_INSTALL_PLUGINS");
    qtEnvironment.qmlImportPath = queryOutput.value("QT_INSTALL_IMPORTS");
    qtEnvironment.qtVersion = queryOutput.value("QT_VERSION");

    QByteArray mkspecsPath = queryOutput.value("QMAKE_MKSPECS");
    if (mkspecsPath.isEmpty()) {
        mkspecsPath = queryOutput.value("QT_INSTALL_PREFIX");
        if (mkspecsPath.isEmpty())
            throw Exception(tr("Cannot extract the mkspecs directory."));
        mkspecsPath += "/mkspecs";
    }
    qtEnvironment.mkspecsPath = mkspecsPath;

    QByteArray qconfigContent = mkSpecContent(mkspecsPath);
    qtEnvironment.qtMajorVersion = configVariable(qconfigContent, "QT_MAJOR_VERSION").toInt();
    qtEnvironment.qtMinorVersion = configVariable(qconfigContent, "QT_MINOR_VERSION").toInt();
    qtEnvironment.qtPatchVersion = configVariable(qconfigContent, "QT_PATCH_VERSION").toInt();
    qtEnvironment.qtNameSpace = configVariable(qconfigContent, "QT_NAMESPACE");
    qtEnvironment.qtLibInfix = configVariable(qconfigContent, "QT_LIBINFIX");
    qtEnvironment.mkspec = mkSpecPath(mkspecsPath);

    if (!QFileInfo(qtEnvironment.mkspec).exists())
        throw Exception(tr("mkspec '%1' does not exist").arg(qtEnvironment.mkspec));

    return qtEnvironment;
}

void SetupQt::saveToQbsSettings(const QString &qtVersionName, const QtEnviroment &qtEnviroment)
{
    QString msg = QCoreApplication::translate("SetupQt", "Creating profile '%0'.").arg(qtVersionName);
    printf("%s\n", qPrintable(msg));

    QSettings qbsSettings(QLatin1String("Nokia"), QLatin1String("qbs"));
    QString settingsTemplate(qtVersionName + QLatin1String("/qt/core/%1"));

    qbsSettings.beginGroup(QLatin1String("profiles"));
    qbsSettings.setValue(settingsTemplate.arg("binPath"), qtEnviroment.binaryPath);
    qbsSettings.setValue(settingsTemplate.arg("libPath"), qtEnviroment.libaryPath);
    qbsSettings.setValue(settingsTemplate.arg("incPath"), qtEnviroment.includePath);
    qbsSettings.setValue(settingsTemplate.arg("mkspecsPath"), qtEnviroment.mkspecsPath);
    qbsSettings.setValue(settingsTemplate.arg("version"), qtEnviroment.qtVersion);
    qbsSettings.setValue(settingsTemplate.arg("namespace"), qtEnviroment.qtNameSpace);
    qbsSettings.setValue(settingsTemplate.arg("libInfix"), qtEnviroment.qtLibInfix);
    qbsSettings.endGroup();
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
