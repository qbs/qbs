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
#include "probe.h"

#include "clangclprobe.h"
#include "gccprobe.h"
#include "iarewprobe.h"
#include "keilprobe.h"
#include "msvcprobe.h"
#include "sdccprobe.h"
#include "xcodeprobe.h"

#include <logging/translator.h>
#include <tools/error.h>
#include <tools/hostosinfo.h>
#include <tools/profile.h>
#include <tools/qttools.h>
#include <tools/settings.h>
#include <tools/toolchains.h>

#include <QtCore/qdir.h>
#include <QtCore/qoperatingsystemversion.h>
#include <QtCore/qtextstream.h>

#ifdef Q_OS_WIN
// We need defines for Windows 8.
#undef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WIN8

#include <qt_windows.h>
#include <shlobj.h>
#else
#include <qplatformdefs.h>
#endif // Q_OS_WIN

using namespace qbs;
using Internal::HostOsInfo;
using Internal::Tr;

static QTextStream qStdout(stdout);
static QTextStream qStderr(stderr);

QStringList systemSearchPaths()
{
    return QString::fromLocal8Bit(qgetenv("PATH")).split(HostOsInfo::pathListSeparator());
}

QString findExecutable(const QString &fileName)
{
    QString fullFileName = fileName;
    if (HostOsInfo::isWindowsHost()
            && !fileName.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
        fullFileName += QLatin1String(".exe");
    }
    const auto ppaths = systemSearchPaths();
    for (const QString &ppath : ppaths) {
        const QString fullPath = ppath + QLatin1Char('/') + fullFileName;
        if (QFileInfo::exists(fullPath))
            return QDir::cleanPath(fullPath);
    }
    return {};
}

QString toolchainTypeFromCompilerName(const QString &compilerName)
{
    if (compilerName == QLatin1String("cl.exe"))
        return QStringLiteral("msvc");
    if (compilerName == QLatin1String("clang-cl.exe"))
        return QStringLiteral("clang-cl");
    const auto types = { QStringLiteral("clang"), QStringLiteral("llvm"),
                         QStringLiteral("mingw"), QStringLiteral("gcc") };
    for (const auto &type : types) {
        if (compilerName.contains(type))
            return type;
    }
    if (compilerName == QLatin1String("g++"))
        return QStringLiteral("gcc");
    if (isIarCompiler(compilerName))
        return QStringLiteral("iar");
    if (isKeilCompiler(compilerName))
        return QStringLiteral("keil");
    if (isSdccCompiler(compilerName))
        return QStringLiteral("sdcc");
    return {};
}

void probe(Settings *settings)
{
    std::vector<Profile> profiles;
    if (HostOsInfo::isWindowsHost()) {
        msvcProbe(settings, profiles);
        clangClProbe(settings, profiles);
    } else if (HostOsInfo::isMacosHost()) {
        xcodeProbe(settings, profiles);
    }

    gccProbe(settings, profiles, QStringLiteral("gcc"));
    gccProbe(settings, profiles, QStringLiteral("clang"));

    iarProbe(settings, profiles);
    keilProbe(settings, profiles);
    sdccProbe(settings, profiles);

    if (profiles.empty()) {
        qStderr << Tr::tr("Could not detect any toolchains. No profile created.") << Qt::endl;
    } else if (profiles.size() == 1 && settings->defaultProfile().isEmpty()) {
        const QString profileName = profiles.front().name();
        qStdout << Tr::tr("Making profile '%1' the default.").arg(profileName) << Qt::endl;
        settings->setValue(QStringLiteral("defaultProfile"), profileName);
    }
}

void createProfile(const QString &profileName, const QString &toolchainType,
                   const QString &compilerFilePath, Settings *settings)
{
    QFileInfo compiler(compilerFilePath);
    if (compilerFilePath == compiler.fileName() && !compiler.exists())
        compiler = QFileInfo(findExecutable(compilerFilePath));

    if (!compiler.exists()) {
        throw qbs::ErrorInfo(Tr::tr("Compiler '%1' not found")
                             .arg(compilerFilePath));
    }

    const QString realToolchainType = !toolchainType.isEmpty()
            ? toolchainType
            : toolchainTypeFromCompilerName(compiler.fileName());
    const QStringList toolchain = canonicalToolchain(realToolchainType);

    if (toolchain.contains(QLatin1String("msvc")))
        createMsvcProfile(compiler, settings, profileName);
    else if (toolchain.contains(QLatin1String("clang-cl")))
        createClangClProfile(compiler, settings, profileName);
    else if (toolchain.contains(QLatin1String("gcc")))
        createGccProfile(compiler, settings, realToolchainType, profileName);
    else if (toolchain.contains(QLatin1String("iar")))
        createIarProfile(compiler, settings, profileName);
    else if (toolchain.contains(QLatin1String("keil")))
        createKeilProfile(compiler, settings, profileName);
    else if (toolchain.contains(QLatin1String("sdcc")))
        createSdccProfile(compiler, settings, profileName);
    else
        throw qbs::ErrorInfo(Tr::tr("Cannot create profile: Unknown toolchain type."));
}

int extractVersion(const QByteArray &macroDump, const QByteArray &keyToken)
{
    const int startIndex = macroDump.indexOf(keyToken);
    if (startIndex == -1)
        return -1;
    const int endIndex = macroDump.indexOf('\n', startIndex);
    if (endIndex == -1)
        return -1;
    const auto keyLength = keyToken.length();
    const int version = macroDump.mid(startIndex + keyLength,
                                      endIndex - startIndex - keyLength)
            .toInt();
    return version;
}

static QString resolveSymlinks(const QString &filePath)
{
    QFileInfo fi(filePath);
    int links = 16;
    while (links-- && fi.isSymLink())
        fi.setFile(fi.dir(), fi.symLinkTarget());
    if (links <= 0)
        return {};
    return fi.filePath();
}

// Copied from qfilesystemengine_win.cpp.
#ifdef Q_OS_WIN

// File ID for Windows up to version 7.
static inline QByteArray fileIdWin7(HANDLE handle)
{
    BY_HANDLE_FILE_INFORMATION info;
    if (GetFileInformationByHandle(handle, &info)) {
        char buffer[sizeof "01234567:0123456701234567\0"];
        qsnprintf(buffer, sizeof(buffer), "%lx:%08lx%08lx",
                  info.dwVolumeSerialNumber,
                  info.nFileIndexHigh,
                  info.nFileIndexLow);
        return QByteArray(buffer);
    }
    return {};
}

// File ID for Windows starting from version 8.
static QByteArray fileIdWin8(HANDLE handle)
{
    QByteArray result;
    FILE_ID_INFO infoEx = {};
    if (::GetFileInformationByHandleEx(
                handle,
                static_cast<FILE_INFO_BY_HANDLE_CLASS>(18), // FileIdInfo in Windows 8
                &infoEx, sizeof(FILE_ID_INFO))) {
        result = QByteArray::number(infoEx.VolumeSerialNumber, 16);
        result += ':';
        // Note: MinGW-64's definition of FILE_ID_128 differs from the MSVC one.
        result += QByteArray(reinterpret_cast<const char *>(&infoEx.FileId),
                             int(sizeof(infoEx.FileId))).toHex();
    }
    return result;
}

static QByteArray fileIdWin(HANDLE fHandle)
{
    return QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows8
            ? fileIdWin8(HANDLE(fHandle))
            : fileIdWin7(HANDLE(fHandle));
}

static QByteArray fileId(const QString &filePath)
{
    QByteArray result;
    const HANDLE handle = ::CreateFile(
                reinterpret_cast<const wchar_t*>(filePath.utf16()), 0,
                FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        result = fileIdWin(handle);
        ::CloseHandle(handle);
    }
    return result;
}

static qint64 fileSize(const QString &filePath)
{
    return QFileInfo(filePath).size();
}

#else

static QByteArray fileId(const QString &filePath)
{
    QByteArray result;
    if (Q_UNLIKELY(filePath.isEmpty()))
        return {};
    QT_STATBUF statResult = {};
    if (QT_STAT(filePath.toLocal8Bit().constData(), &statResult))
        return {};
    result = QByteArray::number(quint64(statResult.st_dev), 16);
    result += ':';
    result += QByteArray::number(quint64(statResult.st_ino));
    return result;
}

#endif // Q_OS_WIN

bool isSameExecutable(const QString &filePath1, const QString &filePath2)
{
    if (filePath1 == filePath2)
        return true;
    if (resolveSymlinks(filePath1) == resolveSymlinks(filePath2))
        return true;
    if (fileId(filePath1) == fileId(filePath2))
        return true;

#ifdef Q_OS_WIN
    if (fileSize(filePath1) == fileSize(filePath2))
        return true;
#endif

    return false;
}
