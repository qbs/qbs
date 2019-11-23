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

#include "processutils.h"

#if defined(Q_OS_WIN)
#   define PSAPI_VERSION 1      // To use GetModuleFileNameEx from Psapi.lib on all Win versions.
#   include <QtCore/qt_windows.h>
#   include <psapi.h>
#elif defined(Q_OS_DARWIN)
#   include <libproc.h>
#elif defined(Q_OS_LINUX)
#   include "fileinfo.h"
#   include <unistd.h>
#   include <cstdio>
#elif defined(Q_OS_BSD4)
# include <QtCore/qfile.h>
#   include <sys/cdefs.h>
#   include <sys/param.h>
#   include <sys/sysctl.h>
# if !defined(Q_OS_NETBSD)
#   include <sys/user.h>
# endif
#else
#   error Missing implementation of processNameByPid for this platform.
#endif

namespace qbs {
namespace Internal {

QString processNameByPid(qint64 pid)
{
#if defined(Q_OS_WIN)
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, DWORD(pid));
    if (!hProcess)
        return {};
    wchar_t buf[UNICODE_STRING_MAX_CHARS];
    const DWORD length = GetModuleFileNameEx(hProcess, NULL, buf, sizeof(buf) / sizeof(wchar_t));
    CloseHandle(hProcess);
    if (!length)
        return {};
    QString name = QString::fromWCharArray(buf, length);
    int i = name.lastIndexOf(QLatin1Char('\\'));
    if (i >= 0)
        name.remove(0, i + 1);
    i = name.lastIndexOf(QLatin1Char('.'));
    if (i >= 0)
        name.truncate(i);
    return name;
#elif defined(Q_OS_DARWIN)
    char name[1024];
    proc_name(pid, name, sizeof(name) / sizeof(char));
    return QString::fromUtf8(name);
#elif defined(Q_OS_LINUX)
    char exePath[64];
    char buf[PATH_MAX];
    memset(buf, 0, sizeof(buf));
    sprintf(exePath, "/proc/%lld/exe", pid);
    if (readlink(exePath, buf, sizeof(buf)) < 0)
        return {};
    return FileInfo::fileName(QString::fromUtf8(buf));
#elif defined(Q_OS_BSD4)
# if defined(Q_OS_NETBSD)
    struct kinfo_proc2 kp;
    int mib[6] = { CTL_KERN, KERN_PROC2, KERN_PROC_PID, (int)pid, sizeof(struct kinfo_proc2), 1 };
# elif defined(Q_OS_OPENBSD)
    struct kinfo_proc kp;
    int mib[6] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, (int)pid, sizeof(struct kinfo_proc), 1 };
# else
    struct kinfo_proc kp;
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, (int)pid };
# endif
    size_t len = sizeof(kp);
    u_int mib_len = sizeof(mib)/sizeof(u_int);

    if (sysctl(mib, mib_len, &kp, &len, NULL, 0) < 0)
        return {};

# if defined(Q_OS_OPENBSD) || defined(Q_OS_NETBSD)
    if (kp.p_pid != pid)
        return {};
    QString name = QFile::decodeName(kp.p_comm);
# else
    if (kp.ki_pid != pid)
        return {};
    QString name = QFile::decodeName(kp.ki_comm);
# endif
    return name;

#else
    Q_UNUSED(pid);
    return {};
#endif
}

} // namespace Internal
} // namespace qbs
