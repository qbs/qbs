/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#include "setupqt.h"

#include <QByteArrayMatcher>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QSettings>
#include <QtDebug>

namespace qbs {

#ifdef Q_OS_WIN32
const QString qmakeExecutableName(QLatin1String("qmake.exe"));
#else
const QString qmakeExecutableName(QLatin1String("qmake"));
#endif

static QStringList collectQmakePaths()
{
    QStringList qmakePaths;

#ifdef Q_OS_WIN
    const char pathSeparator = ';';
#else
    const char pathSeparator = ':';
#endif

    QByteArray enviromentPath = qgetenv("PATH");
    QList<QByteArray> enviromentPaths = enviromentPath.split(pathSeparator);
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

    if (qmakePath.split("/").last() != QLatin1String("qmake"))
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

static QByteArray qmakeQueryOutput(const QString &qmakePath)
{
    QProcess qmakeProcess;
    qmakeProcess.start(qmakePath, QStringList() << "-query");
    qmakeProcess.waitForFinished();
    return qmakeProcess.readAllStandardOutput();
}

static QByteArray mkSpecContent(const QByteArray &mkSpecPath)
{
    QFile qconfigFile(mkSpecPath + "/qconfig.pri");
    if (qconfigFile.open(QFile::ReadOnly))
        return qconfigFile.readAll();

    return QByteArray();
}

static QByteArray queryVariable(const QByteArray &queryOutput, const QByteArray &key)
{
    int beginIndex = queryOutput.indexOf(key) + key.size() + 1;
    int endIndex = queryOutput.indexOf("\n", beginIndex);

    return queryOutput.mid(beginIndex, endIndex - beginIndex);
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

#ifdef Q_OS_WIN
static QByteArray qmakeConfContent(const QByteArray &mkSpecPath)
{
    QFile qconfigFile(mkSpecPath + "/default/qmake.conf");
    if (qconfigFile.open(QFile::ReadOnly))
        return qconfigFile.readAll();

    return QByteArray();
}
#endif

static QString mkSpecPath(const QByteArray &mkSpecPath)
{
#ifdef Q_OS_WIN
    return configVariable(qmakeConfContent(mkSpecPath), "QMAKESPEC_ORIGINAL");
#else
    Q_ASSERT(QFileInfo(mkSpecPath + "/default").isSymLink());
    return QFileInfo(mkSpecPath + "/default").symLinkTarget();
#endif
}

QtEnviroment SetupQt::fetchEnviroment(const QString &qmakePath)
{
    QtEnviroment qtEnviroment;
    QByteArray queryOutput = qmakeQueryOutput(qmakePath);

    qtEnviroment.installPrefixPath = queryVariable(queryOutput, "QT_INSTALL_PREFIX");
    qtEnviroment.documentationPath = queryVariable(queryOutput, "QT_INSTALL_DOCS");
    qtEnviroment.includePath = queryVariable(queryOutput, "QT_INSTALL_HEADERS");
    qtEnviroment.libaryPath = queryVariable(queryOutput, "QT_INSTALL_LIBS");
    qtEnviroment.binaryPath = queryVariable(queryOutput, "QT_INSTALL_BINS");
    qtEnviroment.pluginPath = queryVariable(queryOutput, "QT_INSTALL_PLUGINS");
    qtEnviroment.qmlImportPath = queryVariable(queryOutput, "QT_INSTALL_IMPORTS");
    qtEnviroment.qtVersion = queryVariable(queryOutput, "QT_VERSION");

    QByteArray mkspecPath = queryVariable(queryOutput, "QMAKE_MKSPECS");
    QByteArray qconfigContent = mkSpecContent(mkspecPath);

    qtEnviroment.qtMajorVersion = configVariable(qconfigContent, "QT_MAJOR_VERSION").toInt();
    qtEnviroment.qtMinorVersion = configVariable(qconfigContent, "QT_MINOR_VERSION").toInt();
    qtEnviroment.qtPatchVersion = configVariable(qconfigContent, "QT_PATCH_VERSION").toInt();
    qtEnviroment.qtNameSpace = configVariable(qconfigContent, "QT_NAMESPACE");
    qtEnviroment.qtLibaryInfix = configVariable(qconfigContent, "QT_LIBINFIX");
    qtEnviroment.mkSpecPath = mkSpecPath(mkspecPath);

    return qtEnviroment;
}

void SetupQt::saveToQbsSettings(const QString &qtVersionName, const QtEnviroment &qtEnviroment)
{
    QSettings qbsSettings(QLatin1String("Nokia"), QLatin1String("qbs"));
    QString settingsTemplate(qtVersionName + QLatin1String("/qt/core/%1"));

    qbsSettings.beginGroup(QLatin1String("profiles"));
    qbsSettings.setValue(settingsTemplate.arg("binPath"), qtEnviroment.binaryPath);
    qbsSettings.setValue(settingsTemplate.arg("libPath"), qtEnviroment.libaryPath);
    qbsSettings.setValue(settingsTemplate.arg("incPath"), qtEnviroment.includePath);
    qbsSettings.setValue(settingsTemplate.arg("mkspecsPath"), qtEnviroment.mkSpecPath);
    qbsSettings.setValue(settingsTemplate.arg("version"), qtEnviroment.qtVersion);
    qbsSettings.setValue(settingsTemplate.arg("namespace"), qtEnviroment.qtNameSpace);
    qbsSettings.setValue(settingsTemplate.arg("libaryInfix"), qtEnviroment.qtLibaryInfix);
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
