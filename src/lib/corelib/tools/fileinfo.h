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

#ifndef QBS_FILEINFO_H
#define QBS_FILEINFO_H

#include "filetime.h"
#include "qbs_export.h"

#if defined(Q_OS_UNIX)
#include <sys/stat.h>
#endif

#include <QString>

QT_FORWARD_DECLARE_CLASS(QFileInfo)

namespace qbs {
namespace Internal {

class FileInfo
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
    static QString path(const QString &fp);
    static void splitIntoDirectoryAndFileName(const QString &filePath, QString *dirPath, QString *fileName);
    static void splitIntoDirectoryAndFileName(const QString &filePath, QStringRef *dirPath, QStringRef *fileName);
    static bool exists(const QString &fp);
    static bool isAbsolute(const QString &fp);
    static bool isPattern(const QStringRef &str);
    static bool isPattern(const QString &str);
    static QString resolvePath(const QString &base, const QString &rel);
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
    typedef struct stat InternalStatType;
#else
#   error unknown platform
#endif
    InternalStatType m_stat;
};

bool removeFileRecursion(const QFileInfo &f, QString *errorMessage);

// FIXME: Used by tests.
bool QBS_EXPORT removeDirectoryWithContents(const QString &path, QString *errorMessage);
bool QBS_EXPORT copyFileRecursion(const QString &sourcePath, const QString &targetPath,
                                  bool preserveSymLinks, QString *errorMessage);

} // namespace Internal
} // namespace qbs

#endif
