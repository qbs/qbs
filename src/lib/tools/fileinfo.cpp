/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "fileinfo.h"

#include <tools/hostosinfo.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <cassert>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

namespace qbs {

QString FileInfo::fileName(const QString &fp)
{
    int last = fp.lastIndexOf(QLatin1Char('/'));
    if (last < 0)
        return fp;
    return fp.mid(last + 1);
}

QString FileInfo::baseName(const QString &fp)
{
    QString fn = fileName(fp);
    int dot = fn.indexOf(QLatin1Char('.'));
    if (dot < 0)
        return fp;
    return fn.mid(0, dot);
}

QString FileInfo::completeBaseName(const QString &fp)
{
    QString fn = fileName(fp);
    int dot = fn.lastIndexOf(QLatin1Char('.'));
    if (dot < 0)
        return fp;
    return fn.mid(0, dot);
}

QString FileInfo::path(const QString &fp)
{
    if (fp.isEmpty())
        return QString();
    if (fp.at(fp.size() - 1) == QLatin1Char('/'))
        return fp;
    int last = fp.lastIndexOf(QLatin1Char('/'));
    if (last < 0)
        return QLatin1String(".");
    return QDir::cleanPath(fp.mid(0, last));
}

void FileInfo::splitIntoDirectoryAndFileName(const QString &filePath, QString *dirPath, QString *fileName)
{
    int idx = filePath.lastIndexOf(QLatin1Char('/'));
    if (idx < 0) {
        dirPath->clear();
        *fileName = filePath;
        return;
    }
    *dirPath = filePath.left(idx);
    *fileName = filePath.mid(idx + 1);
}

void FileInfo::splitIntoDirectoryAndFileName(const QString &filePath, QStringRef *dirPath, QStringRef *fileName)
{
    int idx = filePath.lastIndexOf(QLatin1Char('/'));
    if (idx < 0) {
        dirPath->clear();
        *fileName = QStringRef(&filePath);
        return;
    }
    *dirPath = filePath.leftRef(idx);
    *fileName = filePath.midRef(idx + 1);
}

bool FileInfo::exists(const QString &fp)
{
    return FileInfo(fp).exists();
}

// from creator/src/shared/proparser/ioutils.cpp
bool FileInfo::isAbsolute(const QString &path)
{
    const int n = path.size();
    if (n == 0)
        return false;
    const QChar at0 = path.at(0);
    if (at0 == QLatin1Char('/'))
        return true;
    if (HostOsInfo::isWindowsHost()) {
        if (at0 == QLatin1Char('\\'))
            return true;
        // Unlike QFileInfo, this won't accept a relative path with a drive letter.
        // Such paths result in a royal mess anyway ...
        if (n >= 3 && path.at(1) == QLatin1Char(':') && at0.isLetter()
                && (path.at(2) == QLatin1Char('/') || path.at(2) == QLatin1Char('\\')))
            return true;
    }
    return false;
}

bool FileInfo::isPattern(const QString &str)
{
    return isPattern(QStringRef(&str));
}

bool FileInfo::isPattern(const QStringRef &str)
{
    for (int i = 0; i < str.size(); ++i) {
        const QChar ch = str.at(i);
        if (ch == QLatin1Char('*') || ch == QLatin1Char('?')
                || ch == QLatin1Char(']') || ch == QLatin1Char('[')) {
            return true;
        }
    }
    return false;
}

/**
 * Concatenates the paths \a base and \a rel.
 * Base must be an absolute path.
 * Double dots at the start of \a rel are handled.
 * This function assumes that both paths are clean, that is they don't contain
 * double slashes or redundant dot parts.
 */
QString FileInfo::resolvePath(const QString &base, const QString &rel)
{
    Q_ASSERT(isAbsolute(base));
    if (isAbsolute(rel))
        return rel;
    if (rel.size() == 1 && rel.at(0) == QLatin1Char('.'))
        return base;

    QString r = base;
    if (r.endsWith(QLatin1Char('/')))
        r.chop(1);

    QString s = rel;
    while (s.startsWith(QLatin1String("../"))) {
        s.remove(0, 3);
        int idx = r.lastIndexOf(QLatin1Char('/'));
        if (idx >= 0)
            r.truncate(idx);
    }

    r.reserve(r.length() + 1 + s.length());
    r += QLatin1Char('/');
    r += s;
    return r;
}

bool FileInfo::globMatches(const QRegExp &regexp, const QString &fileName)
{
    const QString pattern = regexp.pattern();
    // May be it's simple wildcard, i.e. "*.cpp"?
    if (pattern.startsWith(QLatin1Char('*')) && !isPattern(pattern.midRef(1))) {
        // Yes, it's rather simple to just check the extension
        return fileName.endsWith(pattern.midRef(1));
    }
    return regexp.exactMatch(fileName);
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

static QString resolveSymlinks(const QString &fileName)
{
    QFileInfo fi(fileName);
    while (fi.isSymLink())
        fi.setFile(fi.symLinkTarget());
    return fi.absoluteFilePath();
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
bool removeFileRecursion(const QFileInfo &f, QString *errorMessage)
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

/*!
  Copies the directory specified by \a srcFilePath recursively to \a tgtFilePath.
  \a tgtFilePath will contain the target directory, which will be created. Example usage:

  \code
    QString error;
    book ok = Utils::FileUtils::copyRecursively("/foo/bar", "/foo/baz", &error);
    if (!ok)
      qDebug() << error;
  \endcode

  This will copy the contents of /foo/bar into to the baz directory under /foo,
  which will be created in the process.

  \return Whether the operation succeeded.
  \note Function was adapted from qtc/src/libs/fileutils.cpp
*/

bool copyFileRecursion(const QString &srcFilePath, const QString &tgtFilePath,
    QString *errorMessage)
{
    QFileInfo srcFileInfo(srcFilePath);
    if (srcFileInfo.isDir()) {
        QDir targetDir(tgtFilePath);
        targetDir.cdUp();
        if (!targetDir.mkpath(QFileInfo(tgtFilePath).fileName())) {
            *errorMessage = FileInfo::tr("The directory '%1' could not be created.")
               .arg(QDir::toNativeSeparators(tgtFilePath));
            return false;
        }
        QDir sourceDir(srcFilePath);
        QStringList fileNames = sourceDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot
            | QDir::Hidden | QDir::System);
        foreach (const QString &fileName, fileNames) {
            const QString newSrcFilePath = srcFilePath + QLatin1Char('/') + fileName;
            const QString newTgtFilePath = tgtFilePath + QLatin1Char('/') + fileName;
            if (!copyFileRecursion(newSrcFilePath, newTgtFilePath, errorMessage))
                return false;
        }
    } else {
        if (!QFile::copy(srcFilePath, tgtFilePath)) {
            *errorMessage = FileInfo::tr("Could not copy file '%1' to '%2'.")
                .arg(QDir::toNativeSeparators(srcFilePath), QDir::toNativeSeparators(tgtFilePath));
            return false;
        }
    }
    return true;
}


QString qbsRootPath()
{
    return QDir::cleanPath(applicationDirPath() + QLatin1String("/../"));
}

}
