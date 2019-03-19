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
#include <tools/stringconstants.h>

#include <QtCore/qbytearray.h>
#include <QtCore/qdir.h>
#include <QtCore/qprocess.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qtemporaryfile.h>

#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif

#include <algorithm>
#include <mutex>

using namespace qbs;
using namespace qbs::Internal;

static std::recursive_mutex envMutex;

static QString mkStr(const char *s) { return QString::fromLocal8Bit(s); }
static QString mkStr(const QByteArray &ba) { return mkStr(ba.constData()); }

class TemporaryEnvChanger
{
public:
    TemporaryEnvChanger(const QProcessEnvironment &envChanges) : m_locker(envMutex)
    {
        QProcessEnvironment currentEnv = QProcessEnvironment::systemEnvironment();
        const auto keys = envChanges.keys();
        for (const QString &key : keys) {
            m_changesToRestore.insert(key, currentEnv.value(key));
            qputenv(qPrintable(key), qPrintable(envChanges.value(key)));
        }
    }

    ~TemporaryEnvChanger()
    {
        const auto keys = m_changesToRestore.keys();
        for (const QString &key : keys)
            qputenv(qPrintable(key), qPrintable(m_changesToRestore.value(key)));
    }

private:
    QProcessEnvironment m_changesToRestore;
    std::lock_guard<std::recursive_mutex> m_locker;
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
    if (!process.waitForFinished(-1) || process.exitStatus() != QProcess::NormalExit)
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
    DummyFile(QString fp) : filePath(std::move(fp)) { }
    ~DummyFile() { QFile::remove(filePath); }
    const QString filePath;
};

#ifdef Q_OS_WIN
static QStringList parseCommandLine(const QString &commandLine)
{
    QStringList list;
    const auto buf = new wchar_t[commandLine.size() + 1];
    buf[commandLine.toWCharArray(buf)] = 0;
    int argCount = 0;
    LPWSTR *args = CommandLineToArgvW(buf, &argCount);
    if (!args)
        throw ErrorInfo(mkStr("Could not parse command line arguments: ") + commandLine);
    for (int i = 0; i < argCount; ++i)
        list.push_back(QString::fromWCharArray(args[i]));
    delete[] buf;
    return list;
}
#endif

static QVariantMap getMsvcDefines(const QString &compilerFilePath,
                                  const QProcessEnvironment &compilerEnv,
                                  MSVC::CompilerLanguage language)
{
#ifdef Q_OS_WIN
    QString backendSwitch, languageSwitch;
    switch (language) {
    case MSVC::CLanguage:
        backendSwitch = QStringLiteral("/B1");
        languageSwitch = QStringLiteral("/TC");
        break;
    case MSVC::CPlusPlusLanguage:
        backendSwitch = QStringLiteral("/Bx");
        languageSwitch = QStringLiteral("/TP");
        break;
    }
    const QByteArray commands("set MSC_CMD_FLAGS\n");
    QStringList out = QString::fromLocal8Bit(runProcess(compilerFilePath, QStringList()
               << QStringLiteral("/nologo")
               << backendSwitch
               << QString::fromWCharArray(_wgetenv(L"COMSPEC"))
               << QStringLiteral("/c")
               << languageSwitch
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
    Q_UNUSED(language);
    return {};
#endif
}

/*!
    \internal
    clang-cl does not support gcc and msvc ways to dump a macros, so we have to use original
    clang.exe directly
*/
static QVariantMap getClangClDefines(
        const QString &compilerFilePath,
        const QProcessEnvironment &compilerEnv,
        MSVC::CompilerLanguage language)
{
#ifdef Q_OS_WIN
    QFileInfo clInfo(compilerFilePath);
    QFileInfo clangInfo(clInfo.absolutePath() + QLatin1String("/clang.exe"));
    if (!clangInfo.exists())
        throw ErrorInfo(QStringLiteral("%1 does not exist").arg(clangInfo.absoluteFilePath()));

    QString languageSwitch;
    switch (language) {
    case MSVC::CLanguage:
        languageSwitch = QStringLiteral("c");
        break;
    case MSVC::CPlusPlusLanguage:
        languageSwitch = QStringLiteral("c++");
        break;
    }
    QStringList args = {
        QStringLiteral("-dM"),
        QStringLiteral("-E"),
        QStringLiteral("-x"),
        languageSwitch,
        QStringLiteral("NUL"),
    };
    const auto lines = QString::fromLocal8Bit(
            runProcess(
                    clangInfo.absoluteFilePath(),
                    args,
                    compilerEnv,
                    true)).split(QLatin1Char('\n'));
    QVariantMap result;
    for (const auto &line: lines) {
        static const auto defineString = QLatin1String("#define ");
        if (!line.startsWith(defineString)) {
            throw ErrorInfo(QStringLiteral("Unexpected compiler frontend output: ")
                            + lines.join(QLatin1Char('\n')));
        }
        QStringView view(line.data() + defineString.size());
        const auto it = std::find(view.begin(), view.end(), QLatin1Char(' '));
        if (it == view.end()) {
            throw ErrorInfo(QStringLiteral("Unexpected compiler frontend output: ")
                            + lines.join(QLatin1Char('\n')));
        }
        QStringView key(view.begin(), it);
        QStringView value(it + 1, view.end());
        result.insert(key.toString(), value.isEmpty() ? QVariant() : QVariant(value.toString()));
    }
    return result;
#else
    Q_UNUSED(compilerFilePath);
    Q_UNUSED(compilerEnv);
    Q_UNUSED(language);
    return {};
#endif
}

void MSVC::init()
{
    determineCompilerVersion();
}

/*!
  \internal
  Returns the architecture detected from the compiler path.
*/
QString MSVC::architectureFromClPath(const QString &clPath)
{
    const auto parentDir = QFileInfo(clPath).absolutePath();
    const auto parentDirName = QFileInfo(parentDir).fileName().toLower();
    if (parentDirName == QLatin1String("bin"))
        return QStringLiteral("x86");
    return parentDirName;
}

QString MSVC::binPathForArchitecture(const QString &arch) const
{
    QString archSubDir;
    if (arch != StringConstants::x86Arch())
        archSubDir = arch;
    return QDir::cleanPath(vcInstallPath + QLatin1Char('/') + pathPrefix + QLatin1Char('/')
                           + archSubDir);
}

static QString clExeSuffix() { return QStringLiteral("/cl.exe"); }

QString MSVC::clPathForArchitecture(const QString &arch) const
{
    return binPathForArchitecture(arch) + clExeSuffix();
}

QVariantMap MSVC::compilerDefines(const QString &compilerFilePath,
                                  MSVC::CompilerLanguage language) const
{
    const auto compilerName = QFileInfo(compilerFilePath).fileName().toLower();
    if (compilerName == QLatin1String("clang-cl.exe"))
        return getClangClDefines(compilerFilePath, environment, language);
    return getMsvcDefines(compilerFilePath, environment, language);
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

    std::lock_guard<std::recursive_mutex> locker(envMutex);
    const QByteArray origPath = qgetenv("PATH");
    qputenv("PATH", environment.value(StringConstants::pathEnvVar()).toLatin1() + ';' + origPath);
    QByteArray versionStr = runProcess(
                binPath + clExeSuffix(),
                QStringList() << QStringLiteral("/nologo") << QStringLiteral("/EP")
                << QDir::toNativeSeparators(cppFilePath));
    qputenv("PATH", origPath);
    compilerVersion = Version(versionStr.mid(0, 2).toInt(), versionStr.mid(2, 2).toInt(),
                              versionStr.mid(4).toInt());
}
