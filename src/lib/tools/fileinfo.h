/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#ifndef FILEINFO_H
#define FILEINFO_H

#include "filetime.h"

#if defined(Q_OS_UNIX)
#include <sys/stat.h>
#endif

#include <QtCore/QString>
#include <QtCore/QCoreApplication> // for Q_DECLARE_TR_FUNCTIONS

QT_FORWARD_DECLARE_CLASS(QFileInfo)

namespace qbs {

class FileInfo
{
    Q_DECLARE_TR_FUNCTIONS(qbs::FileInfo)
public:
    FileInfo(const QString &fileName);

    bool exists() const;
    FileTime lastModified() const;

    static QString fileName(const QString &fp);
    static QString baseName(const QString &fp);
    static QString completeBaseName(const QString &fp);
    static QString path(const QString &fp);
    static bool exists(const QString &fp);
    static bool isAbsolute(const QString &fp);
    static QString resolvePath(const QString &base, const QString &rel);
    static bool globMatches(const QRegExp &pattern, const QString &subject);

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

QString applicationDirPath();
QString qbsRootPath();
bool removeFileRecursion(const QFileInfo &f, QString *errorMessage);
bool removeDirectoryWithContents(const QString &path, QString *errorMessage);
bool copyFileRecursion(const QString &sourcePath, const QString &targetPath, QString *errorMessage);

}

#endif
