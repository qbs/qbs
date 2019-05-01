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

#ifndef QBS_FILEINFO_H
#define QBS_FILEINFO_H

#include "filetime.h"
#include "hostosinfo.h"
#include "qbs_export.h"

#if defined(Q_OS_UNIX)
#include <sys/stat.h>
#endif

#include <QtCore/qstring.h>

QT_FORWARD_DECLARE_CLASS(QFileInfo)

namespace qbs {
namespace Internal {

class QBS_AUTOTEST_EXPORT FileInfo
{
public:
    FileInfo(const QString &fileName);

    bool exists() const;
    FileTime lastModified() const;
    FileTime lastStatusChange() const;
    bool isDir() const;

    static QString fileName(const QString &fp);
    static QString baseName(const QString &fp);
    static QString completeBaseName(const QString &fp);
    static QString suffix(const QString &fp);
    static QString completeSuffix(const QString &fp);
    static QString path(const QString &fp, HostOsInfo::HostOs hostOs = HostOsInfo::hostOs());
    static void splitIntoDirectoryAndFileName(const QString &filePath, QString *dirPath, QString *fileName);
    static void splitIntoDirectoryAndFileName(const QString &filePath, QStringRef *dirPath, QStringRef *fileName);
    static bool exists(const QString &fp);
    static bool isAbsolute(const QString &fp, HostOsInfo::HostOs hostOs = HostOsInfo::hostOs());
    static bool isPattern(const QStringRef &str);
    static bool isPattern(const QString &str);
    static QString resolvePath(const QString &base, const QString &rel,
                               HostOsInfo::HostOs hostOs = HostOsInfo::hostOs());
    static bool globMatches(const QRegExp &pattern, const QString &subject);
    static bool isFileCaseCorrect(const QString &filePath);

    // Symlink-correct check.
    static bool fileExists(const QFileInfo &fi);

private:
#if defined(Q_OS_WIN)
    struct InternalStatType
    {
        quint8 z[36];
    };
#elif defined(Q_OS_UNIX)
    using InternalStatType = struct stat;
#else
#   error unknown platform
#endif
    InternalStatType m_stat{};
};

bool removeFileRecursion(const QFileInfo &f, QString *errorMessage);

// FIXME: Used by tests.
bool QBS_EXPORT removeDirectoryWithContents(const QString &path, QString *errorMessage);
bool QBS_EXPORT copyFileRecursion(const QString &sourcePath, const QString &targetPath,
                                  bool preserveSymLinks, bool copyDirectoryContents, QString *errorMessage);

} // namespace Internal
} // namespace qbs

#endif
