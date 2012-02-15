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

#include "fileinfo.h"
#include <QtCore/QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <cassert>


#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

namespace qbs {

QString FileInfo::fileName(const QString &fp)
{
    int last = fp.lastIndexOf('/');
    if (last < 0)
        return fp;
    return fp.mid(last + 1);
}

QString FileInfo::baseName(const QString &fp)
{
    QString fn = fileName(fp);
    int dot = fn.indexOf('.');
    if (dot < 0)
        return fp;
    return fn.mid(0, dot);
}

QString FileInfo::completeBaseName(const QString &fp)
{
    QString fn = fileName(fp);
    int dot = fn.lastIndexOf('.');
    if (dot < 0)
        return fp;
    return fn.mid(0, dot);
}

QString FileInfo::path(const QString &fp)
{
    if (fp.isEmpty())
        return QString();
    if (fp.at(fp.size() - 1) == '/')
        return fp;
    int last = fp.lastIndexOf('/');
    if (last < 0)
        return ".";
    return QDir::cleanPath(fp.mid(0, last));
}

bool FileInfo::exists(const QString &fp)
{
    return FileInfo(fp).exists();
}

// from creator/src/shared/proparser/ioutils.cpp
bool FileInfo::isAbsolute(const QString &path)
{
    if (path.startsWith(QLatin1Char('/')))
        return true;
#ifdef Q_OS_WIN
    if (path.startsWith(QLatin1Char('\\')))
        return true;
    // Unlike QFileInfo, this won't accept a relative path with a drive letter.
    // Such paths result in a royal mess anyway ...
    if (path.length() >= 3 && path.at(1) == QLatin1Char(':') && path.at(0).isLetter()
            && (path.at(2) == QLatin1Char('/') || path.at(2) == QLatin1Char('\\')))
        return true;
#endif
    return false;
}

QString FileInfo::resolvePath(const QString &base, const QString &rel)
{
    if (isAbsolute(rel))
        return rel;
    if (rel == QLatin1String("."))
        return base;

    QString r = base;
    if (!r.endsWith('/')) {
        r.append('/');
    }
    r.append(rel);

    return r;
}

bool FileInfo::globMatches(const QString &pattern, const QString &subject)
{
    // ### the QRegExp wildcard matcher is slow!
    //QRegExp rex(pattern, Qt::CaseSensitive, QRegExp::Wildcard);
    //return rex.exactMatch(subject);
    return subject.endsWith(pattern.mid(1), Qt::CaseInsensitive);
}

static QString resolveSymlinks(const QString &fileName)
{
    QFileInfo fi(fileName);
    while (fi.isSymLink())
        fi.setFile(fi.symLinkTarget());
    return fi.absoluteFilePath();
}

#if defined(Q_OS_WIN)

#include <qt_windows.h>

#define z(x) reinterpret_cast<WIN32_FILE_ATTRIBUTE_DATA*>(const_cast<FileInfo::InternalStatType*>(&x))

template<bool> struct CompileTimeAssert;
template<> struct CompileTimeAssert<true> {};

FileInfo::FileInfo(const QString &fileName)
{
    static CompileTimeAssert<
        sizeof(FileInfo::InternalStatType) == sizeof(WIN32_FILE_ATTRIBUTE_DATA)
            > internal_type_has_wrong_size;
    if (!GetFileAttributesEx(reinterpret_cast<const WCHAR*>(fileName.utf16()),
                             GetFileExInfoStandard, &m_stat))
    {
        z(m_stat)->dwFileAttributes = INVALID_FILE_ATTRIBUTES;
    }
}

bool FileInfo::exists() const
{
    return z(m_stat)->dwFileAttributes != INVALID_FILE_ATTRIBUTES;
}

FileTime FileInfo::lastModified() const
{
    return FileTime(*reinterpret_cast<const FileTime::InternalType*>(
        &z(m_stat)->ftLastWriteTime));
}

QString applicationDirPath()
{
    static const QString appDirPath = FileInfo::path(resolveSymlinks(QCoreApplication::applicationFilePath()));
    return appDirPath;
}

#elif defined(Q_OS_UNIX)

FileInfo::FileInfo(const QString &fileName)
{
    if (stat(fileName.toLocal8Bit(), &m_stat) == -1)
        m_stat.st_mtime = 0;
}

bool FileInfo::exists() const
{
    return m_stat.st_mtime != 0;
}

FileTime FileInfo::lastModified() const
{
    return m_stat.st_mtime;
}

QString applicationDirPath()
{
    return QCoreApplication::applicationDirPath();
}

#endif

// adapted from qtc/plugins/vcsbase/cleandialog.cpp
static bool removeFileRecursion(const QFileInfo &f, QString *errorMessage)
{
    if (!f.exists())
        return true;
    if (f.isDir()) {
        const QDir dir(f.absoluteFilePath());
        foreach(const QFileInfo &fi, dir.entryInfoList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden))
            removeFileRecursion(fi, errorMessage);
        QDir parent = f.absoluteDir();
        if (!parent.rmdir(f.fileName())) {
            errorMessage->append(FileInfo::tr("The directory %1 could not be deleted.").
                                 arg(QDir::toNativeSeparators(f.absoluteFilePath())));
            return false;
        }
    } else if (!QFile::remove(f.absoluteFilePath())) {
        if (!errorMessage->isEmpty())
            errorMessage->append(QLatin1Char('\n'));
        errorMessage->append(FileInfo::tr("The file %1 could not be deleted.").
                             arg(QDir::toNativeSeparators(f.absoluteFilePath())));
        return false;
    }
    return true;
}

bool removeDirectoryWithContents(const QString &path, QString *errorMessage)
{
    QFileInfo f(path);
    if (f.exists() && !f.isDir()) {
        *errorMessage = FileInfo::tr("%1 is not a directory.").arg(QDir::toNativeSeparators(path));
        return false;
    }
    return removeFileRecursion(f, errorMessage);
}

QString qbsRootPath()
{
    return QDir::cleanPath(applicationDirPath() + QLatin1String("/../"));
}

}
