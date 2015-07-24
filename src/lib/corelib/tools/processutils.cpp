/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
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

#include "processutils.h"

#if defined(Q_OS_WIN)
#   define PSAPI_VERSION 1      // To use GetModuleFileNameEx from Psapi.lib on all Win versions.
#   include <qt_windows.h>
#   include <Psapi.h>
#elif defined(Q_OS_DARWIN)
#   include <libproc.h>
#elif defined(Q_OS_LINUX)
#   include "fileinfo.h"
#   include <unistd.h>
#   include <cstdio>
#elif defined(Q_OS_BSD4)
#   include <libutil.h>
#   include <sys/types.h>
#   include <sys/user.h>
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
        return QString();
    wchar_t buf[MAX_PATH];
    const DWORD length = GetModuleFileNameEx(hProcess, NULL, buf, sizeof(buf) / sizeof(wchar_t));
    CloseHandle(hProcess);
    if (!length)
        return QString();
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
    readlink(exePath, buf, sizeof(buf));
    return FileInfo::fileName(QString::fromUtf8(buf));
#elif defined(Q_OS_BSD4)
    kinfo_proc *proc = kinfo_getproc(pid);
    if (!proc)
        return QString();
    QString name = QString::fromUtf8(proc->ki_comm);
    free(proc);
    return name;
#else
    return QString();
#endif
}

} // namespace Internal
} // namespace qbs
