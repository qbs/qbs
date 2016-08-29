/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "msvcinfo.h"

#include <tools/error.h>
#include <tools/profile.h>
#include <tools/version.h>
#include <tools/vsenvironmentdetector.h>

#include <QByteArray>
#include <QDir>
#include <QProcess>
#include <QScopedPointer>
#include <QStringList>
#include <QTemporaryFile>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

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
                             const QProcessEnvironment &env = QProcessEnvironment(),
                             bool allowFailure = false)
{
    TemporaryEnvChanger envChanger(env);
    QProcess process;
    process.start(exeFilePath, args);
    if (!process.waitForStarted() || !process.waitForFinished()
            || process.exitStatus() != QProcess::NormalExit) {
        throw ErrorInfo(mkStr("Could not run %1 (%2)").arg(exeFilePath, process.errorString()));
    }
    if (process.exitCode() != 0 && !allowFailure) {
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

static QStringList parseCommandLine(const QString &commandLine)
{
    QStringList list;
#ifdef Q_OS_WIN
    wchar_t *buf = new wchar_t[commandLine.size() + 1];
    buf[commandLine.toWCharArray(buf)] = 0;
    int argCount = 0;
    LPWSTR *args = CommandLineToArgvW(buf, &argCount);
    if (!args)
        throw ErrorInfo(mkStr("Could not parse command line arguments: ") + commandLine);
    for (int i = 0; i < argCount; ++i)
        list.append(QString::fromWCharArray(args[i]));
    delete[] buf;
#else
    Q_UNUSED(commandLine);
#endif
    return list;
}

static QVariantMap getMsvcDefines(const QString &hostCompilerFilePath,
                                  const QString &compilerFilePath,
                                  const QProcessEnvironment &compilerEnv)
{
    const QScopedPointer<QTemporaryFile> dummyFile(
                new QTemporaryFile(QDir::tempPath() + QLatin1String("/qbs_dummy")));
    if (!dummyFile->open()) {
        throw ErrorInfo(mkStr("Could not create temporary file (%1)")
                        .arg(dummyFile->errorString()));
    }
    dummyFile->write("#include <stdio.h>\n");
    dummyFile->write("#include <stdlib.h>\n");
    dummyFile->write("int main(void) { char *p = getenv(\"MSC_CMD_FLAGS\");"
                     "if (p) printf(\"%s\", p); return EXIT_FAILURE; }\n");
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
    const QString qbsClFrontend = nativeDummyFilePath + QStringLiteral(".exe");

    // The host compiler is the x86 compiler, which will execute on any edition of Windows
    // for which host compilers have been released so far (x86, x86_64, ia64)
    MSVC msvc2(hostCompilerFilePath);
    VsEnvironmentDetector envdetector(&msvc2);
    if (!envdetector.start())
        throw ErrorInfo(QStringLiteral("Detecting the MSVC build environment failed: ")
                        + envdetector.errorString());
    runProcess(hostCompilerFilePath, QStringList()
               << QStringLiteral("/nologo")
               << QStringLiteral("/TC")
               << nativeDummyFilePath
               << QStringLiteral("/link")
               << (QStringLiteral("/out:") + qbsClFrontend), msvc2.environments[QString()]);

    QStringList out = QString::fromLocal8Bit(runProcess(compilerFilePath, QStringList()
               << QStringLiteral("/nologo")
               << QStringLiteral("/B1")
               << qbsClFrontend
               << QStringLiteral("/c")
               << QStringLiteral("/TC")
               << QStringLiteral("NUL"), compilerEnv, true)).split(QStringLiteral("\r\n"));

    if (out.size() != 2)
        throw ErrorInfo(QStringLiteral("Unexpected compiler frontend output: ")
                        + out.join(QLatin1Char('\n')));

    if (out.first() == QStringLiteral("NUL"))
        out.removeFirst();

    QVariantMap map;
    const QStringList args = parseCommandLine(out.first());
    for (const QString &arg : args) {
        if (!arg.startsWith(QStringLiteral("-D")))
            continue;
        int idx = arg.indexOf(QLatin1Char('='), 2);
        if (idx > 2)
            map.insert(arg.mid(2, idx - 2), arg.mid(idx + 1));
        else
            map.insert(arg.mid(2), QVariant());
    }

    return map;
}

QVariantMap MSVC::compilerDefines(const QString &compilerFilePath) const
{
    // Should never happen
    if (architectures.size() != 1)
        throw ErrorInfo(mkStr("Unexpected number of architectures"));

    return getMsvcDefines(clPath(), compilerFilePath, environments[architectures.first()]);
}
