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

#include <QtCore/qbytearray.h>
#include <QtCore/qdir.h>
#include <QtCore/qprocess.h>
#include <QtCore/qscopedpointer.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qtemporaryfile.h>

#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif

#include <algorithm>

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
        for (const QString &key : envChanges.keys()) {
            m_changesToRestore.insert(key, currentEnv.value(key));
            qputenv(qPrintable(key), qPrintable(envChanges.value(key)));
        }
    }

    ~TemporaryEnvChanger()
    {
        for (const QString &key : m_changesToRestore.keys())
            qputenv(qPrintable(key), qPrintable(m_changesToRestore.value(key)));
    }

private:
    QProcessEnvironment m_changesToRestore;
};

static QByteArray runProcess(const QString &exeFilePath, const QStringList &args,
                             const QProcessEnvironment &env = QProcessEnvironment(),
                             bool allowFailure = false,
                             const QByteArray &pipeData = QByteArray())
{
    TemporaryEnvChanger envChanger(env);
    QProcess process;
    process.start(exeFilePath, args);
    if (!process.waitForStarted())
        throw ErrorInfo(mkStr("Could not start %1 (%2)").arg(exeFilePath, process.errorString()));
    if (!pipeData.isEmpty()) {
        process.write(pipeData);
        process.closeWriteChannel();
    }
    if (!process.waitForFinished() || process.exitStatus() != QProcess::NormalExit)
        throw ErrorInfo(mkStr("Could not run %1 (%2)").arg(exeFilePath, process.errorString()));
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

#ifdef Q_OS_WIN
static QStringList parseCommandLine(const QString &commandLine)
{
    QStringList list;
    wchar_t *buf = new wchar_t[commandLine.size() + 1];
    buf[commandLine.toWCharArray(buf)] = 0;
    int argCount = 0;
    LPWSTR *args = CommandLineToArgvW(buf, &argCount);
    if (!args)
        throw ErrorInfo(mkStr("Could not parse command line arguments: ") + commandLine);
    for (int i = 0; i < argCount; ++i)
        list.append(QString::fromWCharArray(args[i]));
    delete[] buf;
    return list;
}
#endif

static QVariantMap getMsvcDefines(const QString &compilerFilePath,
                                  const QProcessEnvironment &compilerEnv)
{
#ifdef Q_OS_WIN
    const QByteArray commands("set MSC_CMD_FLAGS\n");
    QStringList out = QString::fromLocal8Bit(runProcess(compilerFilePath, QStringList()
               << QStringLiteral("/nologo")
               << QStringLiteral("/B1")
               << QString::fromWCharArray(_wgetenv(L"COMSPEC"))
               << QStringLiteral("/c")
               << QStringLiteral("/TC")
               << QStringLiteral("NUL"),
               compilerEnv, true, commands)).split(QLatin1Char('\n'));

    auto findResult = std::find_if(out.cbegin(), out.cend(), [] (const QString &line) {
            return line.startsWith(QLatin1String("MSC_CMD_FLAGS="));
        });
    if (findResult == out.cend()) {
        throw ErrorInfo(QStringLiteral("Unexpected compiler frontend output: ")
                        + out.join(QLatin1Char('\n')));
    }

    QVariantMap map;
    const QStringList args = parseCommandLine(findResult->trimmed());
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
#else
    Q_UNUSED(compilerFilePath);
    Q_UNUSED(compilerEnv);
    return QVariantMap();
#endif
}

void MSVC::init()
{
    determineCompilerVersion();
}

QString MSVC::binPathForArchitecture(const QString &arch) const
{
    QString archSubDir;
    if (arch != QStringLiteral("x86"))
        archSubDir = arch;
    return QDir::cleanPath(vcInstallPath + QLatin1Char('/') + pathPrefix + QLatin1Char('/')
                           + archSubDir);
}

QString MSVC::clPathForArchitecture(const QString &arch) const
{
    return binPathForArchitecture(arch) + QLatin1String("/cl.exe");
}

QVariantMap MSVC::compilerDefines(const QString &compilerFilePath) const
{
    return getMsvcDefines(compilerFilePath, environment);
}

void MSVC::determineCompilerVersion()
{
    QString cppFilePath;
    {
        QTemporaryFile cppFile(QDir::tempPath() + QLatin1String("/qbsXXXXXX.cpp"));
        cppFile.setAutoRemove(false);
        if (!cppFile.open()) {
            throw ErrorInfo(mkStr("Could not create temporary file (%1)")
                            .arg(cppFile.errorString()));
        }
        cppFilePath = cppFile.fileName();
        cppFile.write("_MSC_FULL_VER");
        cppFile.close();
    }
    DummyFile fileDeleter(cppFilePath);

    const QByteArray origPath = qgetenv("PATH");
    qputenv("PATH", environment.value(QStringLiteral("PATH")).toLatin1() + ';' + origPath);
    QByteArray versionStr = runProcess(
                binPath + QStringLiteral("/cl.exe"),
                QStringList() << QStringLiteral("/nologo") << QStringLiteral("/EP")
                << QDir::toNativeSeparators(cppFilePath));
    qputenv("PATH", origPath);
    compilerVersion = Version(versionStr.mid(0, 2).toInt(), versionStr.mid(2, 2).toInt(),
                              versionStr.mid(4).toInt());
}
