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
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
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
#include "compilerversion.h"

#include <tools/error.h>
#include <tools/profile.h>
#include <tools/version.h>

#include <QByteArray>
#include <QDir>
#include <QProcess>
#include <QScopedPointer>
#include <QStringList>
#include <QTemporaryFile>

using namespace qbs;
using namespace qbs::Internal;

static QString mkStr(const char *s) { return QString::fromLocal8Bit(s); }
static QString mkStr(const QByteArray &ba) { return mkStr(ba.constData()); }

class TemporaryEnvChanger
{
public:
    TemporaryEnvChanger(const QProcessEnvironment &envChanges)
    {
        QProcessEnvironment currentEnv = QProcessEnvironment::systemEnvironment();
        foreach (const QString &key, envChanges.keys()) {
            m_changesToRestore.insert(key, currentEnv.value(key));
            qputenv(qPrintable(key), qPrintable(envChanges.value(key)));
        }
    }

    ~TemporaryEnvChanger()
    {
        foreach (const QString &key, m_changesToRestore.keys())
            qputenv(qPrintable(key), qPrintable(m_changesToRestore.value(key)));
    }

private:
    QProcessEnvironment m_changesToRestore;
};

static QByteArray runProcess(const QString &exeFilePath, const QStringList &args,
                             const QProcessEnvironment &env = QProcessEnvironment())
{
    TemporaryEnvChanger envChanger(env);
    QProcess process;
    process.start(exeFilePath, args);
    if (!process.waitForStarted() || !process.waitForFinished()
            || process.exitStatus() != QProcess::NormalExit) {
        throw ErrorInfo(mkStr("Could not run %1 (%2)").arg(exeFilePath, process.errorString()));
    }
    if (process.exitCode() != 0) {
        ErrorInfo e(mkStr("Process '%1' failed with exit code %2.")
                    .arg(exeFilePath).arg(process.exitCode()));
        const QByteArray stdErr = process.readAllStandardError();
        if (!stdErr.isEmpty())
            e.append(mkStr("stderr was: %1").arg(mkStr(stdErr)));
        const QByteArray stdOut = process.readAllStandardOutput();
        if (!stdOut.isEmpty())
            e.append(mkStr("stdout was: %1").arg(mkStr(stdOut)));
        throw e;
    }
    return process.readAllStandardOutput().trimmed();
}

class DummyFile {
public:
    DummyFile(const QString &fp) : filePath(fp) { }
    ~DummyFile() { QFile::remove(filePath); }
    const QString filePath;
};

static Version getMsvcVersion(const QString &compilerFilePath,
                              const QProcessEnvironment &compilerEnv)
{
    const QScopedPointer<QTemporaryFile> dummyFile(
                new QTemporaryFile(QDir::tempPath() + QLatin1String("/qbs_dummy")));
    if (!dummyFile->open()) {
        throw ErrorInfo(mkStr("Could not create temporary file (%1)")
                        .arg(dummyFile->errorString()));
    }
    const QByteArray magicPrefix = "int version = ";
    dummyFile->write(magicPrefix + "_MSC_FULL_VER\r\n");
    dummyFile->close();

    // We cannot use the temporary file itself, as Qt has a lock on it
    // even after it was closed, causing a "Permission denied" message from MSVC.
    const QString actualDummyFilePath = dummyFile->fileName() + QLatin1String(".1");
    const QString nativeDummyFilePath = QDir::toNativeSeparators(actualDummyFilePath);
    if (!QFile::copy(dummyFile->fileName(), actualDummyFilePath)) {
        throw ErrorInfo(mkStr("Could not create source '%1' file for compiler.")
                        .arg(nativeDummyFilePath));
    }
    DummyFile actualDummyFile(actualDummyFilePath);
    const QString preprocessedFilePath = nativeDummyFilePath + QLatin1String(".i");
    const QStringList compilerArgs = QStringList() << QLatin1String("/nologo")
            << QLatin1String("/P") << nativeDummyFilePath
            << (QLatin1String("/Fi") + preprocessedFilePath);
    runProcess(compilerFilePath, compilerArgs, compilerEnv);
    QFile preprocessedFile(preprocessedFilePath);
    if (!preprocessedFile.open(QIODevice::ReadOnly)) {
        throw ErrorInfo(mkStr("Cannot read preprocessed file '%1' (%2)")
                        .arg(preprocessedFilePath, preprocessedFile.errorString()));
    }
    QString versionString;
    foreach (const QByteArray &line, preprocessedFile.readAll().split('\n')) {
        const QByteArray cleanLine = line.trimmed();
        if (cleanLine.startsWith(magicPrefix)) {
            versionString = QString::fromLocal8Bit(cleanLine.mid(magicPrefix.count()));
            break;
        }
    }
    if (versionString.isEmpty())
        throw ErrorInfo(mkStr("No version number found in preprocessed file."));
    if (versionString.count() < 5)
        throw ErrorInfo(mkStr("Version number '%1' not understood.").arg(versionString));
    versionString.insert(2, QLatin1Char('.')).insert(5, QLatin1Char('.'));
    const Version version = Version::fromString(versionString);
    if (!version.isValid())
        throw ErrorInfo(mkStr("Invalid version string '%1'.").arg(versionString));
    return version;
}


void setCompilerVersion(const QString &compilerFilePath, const QStringList &qbsToolchain,
                        Profile &profile, const QProcessEnvironment &compilerEnv)
{
    try {
        if (qbsToolchain.contains(QLatin1String("msvc"))) {
            const Version version = getMsvcVersion(compilerFilePath, compilerEnv);
            if (version.isValid()) {
                profile.setValue(QLatin1String("cpp.compilerVersionMajor"), version.majorVersion());
                profile.setValue(QLatin1String("cpp.compilerVersionMinor"), version.minorVersion());
                profile.setValue(QLatin1String("cpp.compilerVersionPatch"), version.patchLevel());
            }
        }
    } catch (const ErrorInfo &e) {
        qDebug("Warning: Failed to retrieve compiler version: %s", qPrintable(e.toString()));
    }
}
