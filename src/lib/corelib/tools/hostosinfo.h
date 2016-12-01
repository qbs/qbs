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

#ifndef QBS_HOSTOSINFO_H
#define QBS_HOSTOSINFO_H

#include "qbs_export.h"
#include "version.h"

#include <QtCore/qglobal.h>
#include <QtCore/qmap.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

#if defined(Q_OS_WIN)
#define QBS_HOST_EXE_SUFFIX ".exe"
#define QBS_HOST_DYNAMICLIB_PREFIX ""
#define QBS_HOST_DYNAMICLIB_SUFFIX ".dll"
#elif defined(Q_OS_DARWIN)
#define QBS_HOST_EXE_SUFFIX ""
#define QBS_HOST_DYNAMICLIB_PREFIX "lib"
#define QBS_HOST_DYNAMICLIB_SUFFIX ".dylib"
#else
#define QBS_HOST_EXE_SUFFIX ""
#define QBS_HOST_DYNAMICLIB_PREFIX "lib"
#define QBS_HOST_DYNAMICLIB_SUFFIX ".so"
#endif // Q_OS_WIN

namespace qbs {
namespace Internal {

class QBS_EXPORT HostOsInfo // Exported for use by command-line tools.
{
public:
    // Add more as needed.
    enum HostOs { HostOsWindows, HostOsLinux, HostOsMacos, HostOsOtherUnix, HostOsOther };

    static inline HostOs hostOs();

    static inline Version hostOsVersion() {
        Version v;
        if (HostOsInfo::isWindowsHost()) {
            QSettings settings(QString::fromLatin1("HKEY_LOCAL_MACHINE\\Software\\"
                                                   "Microsoft\\Windows NT\\CurrentVersion"),
                               QSettings::NativeFormat);
            v = v.fromString(settings.value(QStringLiteral("CurrentVersion")).toString() +
                             QLatin1Char('.') +
                             settings.value(QStringLiteral("CurrentBuildNumber")).toString());
            Q_ASSERT(v.isValid());
        } else if (HostOsInfo::isMacosHost()) {
            QSettings settings(QStringLiteral("/System/Library/CoreServices/SystemVersion.plist"),
                               QSettings::NativeFormat);
            v = v.fromString(settings.value(QStringLiteral("ProductVersion")).toString());
            Q_ASSERT(v.isValid());
        }
        return v;
    }

    static bool isWindowsHost() { return hostOs() == HostOsWindows; }
    static bool isLinuxHost() { return hostOs() == HostOsLinux; }
    static bool isMacosHost() { return hostOs() == HostOsMacos; }
    static inline bool isAnyUnixHost();
    static inline QString rfc1034Identifier(const QString &str);

    static QString appendExecutableSuffix(const QString &executable)
    {
        QString finalName = executable;
        if (isWindowsHost())
            finalName += QLatin1String(QBS_HOST_EXE_SUFFIX);
        return finalName;
    }

    static QString dynamicLibraryName(const QString &libraryBaseName)
    {
        return QLatin1String(QBS_HOST_DYNAMICLIB_PREFIX) + libraryBaseName
                + QLatin1String(QBS_HOST_DYNAMICLIB_SUFFIX);
    }

    static Qt::CaseSensitivity fileNameCaseSensitivity()
    {
        return isWindowsHost() ? Qt::CaseInsensitive: Qt::CaseSensitive;
    }

    static QString libraryPathEnvironmentVariable()
    {
        if (isWindowsHost())
            return QStringLiteral("PATH");
        if (isMacosHost())
            return QStringLiteral("DYLD_LIBRARY_PATH");
        return QStringLiteral("LD_LIBRARY_PATH");
    }

    static QChar pathListSeparator(HostOsInfo::HostOs hostOs = HostOsInfo::hostOs())
    {
        return hostOs == HostOsWindows ? QLatin1Char(';') : QLatin1Char(':');
    }

    static QChar pathSeparator(HostOsInfo::HostOs hostOs = HostOsInfo::hostOs())
    {
        return hostOs == HostOsWindows ? QLatin1Char('\\') : QLatin1Char('/');
    }

    static Qt::KeyboardModifier controlModifier()
    {
        return isMacosHost() ? Qt::MetaModifier : Qt::ControlModifier;
    }
};

HostOsInfo::HostOs HostOsInfo::hostOs()
{
#if defined(Q_OS_WIN)
    return HostOsWindows;
#elif defined(Q_OS_LINUX)
    return HostOsLinux;
#elif defined(Q_OS_DARWIN)
    return HostOsMacos;
#elif defined(Q_OS_UNIX)
    return HostOsOtherUnix;
#else
    return HostOsOther;
#endif
}

bool HostOsInfo::isAnyUnixHost()
{
#ifdef Q_OS_UNIX
    return true;
#else
    return false;
#endif
}

QString HostOsInfo::rfc1034Identifier(const QString &str)
{
    QString s = str;
    for (int i = 0; i < s.size(); ++i) {
        QCharRef ch = s[i];
        const char c = ch.toLatin1();

        const bool okChar = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')
                || (c >= 'a' && c <= 'z') || c == '-' || c == '.';
        if (!okChar)
            ch = QChar::fromLatin1('-');
    }
    return s;
}

} // namespace Internal
} // namespace qbs

#endif // QBS_HOSTOSINFO_H
