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

#include "fileinfo.h"

#include <logging/translator.h>
#include <tools/qbsassert.h>
#include <tools/stringconstants.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qregexp.h>

#if defined(Q_OS_UNIX)
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <QtCore/qt_windows.h>
#endif

namespace qbs {
namespace Internal {

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
        return fn;
    return fn.mid(0, dot);
}

QString FileInfo::completeBaseName(const QString &fp)
{
    QString fn = fileName(fp);
    int dot = fn.lastIndexOf(QLatin1Char('.'));
    if (dot < 0)
        return fn;
    return fn.mid(0, dot);
}

QString FileInfo::suffix(const QString &fp)
{
    QString fn = fileName(fp);
    int dot = fn.lastIndexOf(QLatin1Char('.'));
    if (dot < 0)
        return fn;
    return fn.mid(dot + 1);
}

QString FileInfo::completeSuffix(const QString &fp)
{
    QString fn = fileName(fp);
    int dot = fn.indexOf(QLatin1Char('.'));
    if (dot < 0)
        return fn;
    return fn.mid(dot + 1);
}

QString FileInfo::path(const QString &fp, HostOsInfo::HostOs hostOs)
{
    if (fp.isEmpty())
        return {};
    int last = fp.lastIndexOf(QLatin1Char('/'));
    if (last < 0)
        return StringConstants::dot();
    QString p = QDir::cleanPath(fp.mid(0, last));
    if (p.isEmpty() || (hostOs == HostOsInfo::HostOsWindows && p.length() == 2 && p.at(0).isLetter()
            && p.at(1) == QLatin1Char(':'))) {
        // Make sure we don't return Windows drive roots without an ending slash.
        // Those paths are considered relative.
        p.append(QLatin1Char('/'));
    }
    return p;
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

// Whether a path is the special "current drive path" path type,
// which is neither truly relative nor absolute
static bool isCurrentDrivePath(const QString &path, HostOsInfo::HostOs hostOs)
{
    return hostOs == HostOsInfo::HostOsWindows
            ? path.size() == 2 && path.at(1) == QLatin1Char(':') && path.at(0).isLetter()
            : false;
}

// from creator/src/shared/proparser/ioutils.cpp
bool FileInfo::isAbsolute(const QString &path, HostOsInfo::HostOs hostOs)
{
    const int n = path.size();
    if (n == 0)
        return false;
    const QChar at0 = path.at(0);
    if (at0 == QLatin1Char('/'))
        return true;
    if (hostOs == HostOsInfo::HostOsWindows) {
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
    for (const QChar &ch : str) {
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
QString FileInfo::resolvePath(const QString &base, const QString &rel, HostOsInfo::HostOs hostOs)
{
    QBS_ASSERT(isAbsolute(base, hostOs) && !isCurrentDrivePath(rel, hostOs),
               qDebug("base: %s, rel: %s", qPrintable(base), qPrintable(rel));
            return {});
    if (isAbsolute(rel, hostOs))
        return rel;
    if (rel.size() == 1 && rel.at(0) == QLatin1Char('.'))
        return base;
    if (rel.size() == 1 && rel.at(0) == QLatin1Char('~'))
        return QDir::homePath();
    if (rel.startsWith(StringConstants::tildeSlash()))
        return QDir::homePath() + rel.mid(1);

    QString r = base;
    if (r.endsWith(QLatin1Char('/')))
        r.chop(1);

    QString s = rel;
    while (s.startsWith(QStringLiteral("../"))) {
        s.remove(0, 3);
        int idx = r.lastIndexOf(QLatin1Char('/'));
        if (idx >= 0)
            r.truncate(idx);
    }
    if (s == StringConstants::dotDot()) {
        int idx = r.lastIndexOf(QLatin1Char('/'));
        if (idx >= 0)
            r.truncate(idx);
        s.clear();
    }
    if (!s.isEmpty() || isCurrentDrivePath(r, hostOs)) {
        r.reserve(r.length() + 1 + s.length());
        r += QLatin1Char('/');
        r += s;
    }
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

#ifdef Q_OS_WIN
static QString prependLongPathPrefix(const QString &absolutePath)
{
    QString nativePath = QDir::toNativeSeparators(absolutePath);
    if (nativePath.startsWith(QStringLiteral("\\\\")))
        nativePath.remove(0, 1).prepend(QLatin1String("UNC"));
    nativePath.prepend(QLatin1String("\\\\?\\"));
    return nativePath;
}
#endif

bool FileInfo::isFileCaseCorrect(const QString &filePath)
{
#if defined(Q_OS_WIN)
    // QFileInfo::canonicalFilePath() does not return the real case of the file path on Windows.
    QFileInfo fi(filePath);
    const QString absolute = prependLongPathPrefix(fi.absoluteFilePath());
    WIN32_FIND_DATA fd;
    HANDLE hFindFile = ::FindFirstFile((wchar_t*)absolute.utf16(), &fd);
    if (hFindFile == INVALID_HANDLE_VALUE)
        return false;
    const QString actualFileName = QString::fromWCharArray(fd.cFileName);
    FindClose(hFindFile);
    return actualFileName == fi.fileName();
#elif defined(Q_OS_DARWIN)
    QFileInfo fi(filePath);
    return fi.fileName() == fileName(fi.canonicalFilePath());
#else
    Q_UNUSED(filePath)
    return true;
#endif
}

bool FileInfo::fileExists(const QFileInfo &fi)
{
    return fi.isSymLink() || fi.exists();
}

#if defined(Q_OS_WIN)

#define z(x) reinterpret_cast<WIN32_FILE_ATTRIBUTE_DATA*>(const_cast<FileInfo::InternalStatType*>(&x))

FileInfo::FileInfo(const QString &fileName)
{
    static_assert(sizeof(FileInfo::InternalStatType) == sizeof(WIN32_FILE_ATTRIBUTE_DATA),
                  "FileInfo::InternalStatType has wrong size.");

    QString filePath = fileName;

    // The extended-length path prefix cannot be used with a relative path, so make it absolute
    if (!isAbsolute(filePath))
        filePath = QDir::currentPath() + QDir::separator() + filePath;

    filePath = prependLongPathPrefix(QDir::cleanPath(filePath));
    if (!GetFileAttributesEx(reinterpret_cast<const WCHAR*>(filePath.utf16()),
                             GetFileExInfoStandard, &m_stat))
    {
        ZeroMemory(z(m_stat), sizeof(WIN32_FILE_ATTRIBUTE_DATA));
        z(m_stat)->dwFileAttributes = INVALID_FILE_ATTRIBUTES;
    }
}

bool FileInfo::exists() const
{
    return z(m_stat)->dwFileAttributes != INVALID_FILE_ATTRIBUTES;
}

FileTime FileInfo::lastModified() const
{
    const FileTime::InternalType* ft_it;
    ft_it = reinterpret_cast<const FileTime::InternalType*>(&z(m_stat)->ftLastWriteTime);
    return {*ft_it};
}

FileTime FileInfo::lastStatusChange() const
{
    return lastModified();
}

bool FileInfo::isDir() const
{
    return exists() && z(m_stat)->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
}

static QString resolveSymlinks(const QString &fileName)
{
    QFileInfo fi(fileName);
    while (fi.isSymLink())
        fi.setFile(fi.dir(), fi.symLinkTarget());
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
    if (stat(fileName.toLocal8Bit().constData(), &m_stat) == -1) {
        m_stat.st_mtime = 0;
        m_stat.st_mode = 0;
    }
}

bool FileInfo::exists() const
{
    return m_stat.st_mode != 0;
}

FileTime FileInfo::lastModified() const
{
#if APPLE_STAT_TIMESPEC
    return m_stat.st_mtimespec;
#elif HAS_CLOCK_GETTIME
    return m_stat.st_mtim;
#else
    return m_stat.st_mtime;
#endif
}

FileTime FileInfo::lastStatusChange() const
{
#if APPLE_STAT_TIMESPEC
    return m_stat.st_ctimespec;
#elif HAS_CLOCK_GETTIME
    return m_stat.st_ctim;
#else
    return m_stat.st_ctime;
#endif
}

bool FileInfo::isDir() const
{
    return S_ISDIR(m_stat.st_mode);
}

#endif

// adapted from qtc/plugins/vcsbase/cleandialog.cpp
bool removeFileRecursion(const QFileInfo &f, QString *errorMessage)
{
    if (!FileInfo::fileExists(f))
        return true;
    if (f.isDir() && !f.isSymLink()) {
        const QDir dir(f.absoluteFilePath());

        // QDir::System is needed for broken symlinks.
        const auto fileInfos = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot
                                                 | QDir::Hidden | QDir::System);
        for (const QFileInfo &fi : fileInfos)
            removeFileRecursion(fi, errorMessage);
        QDir parent = f.absoluteDir();
        if (!parent.rmdir(f.fileName())) {
            errorMessage->append(Tr::tr("The directory %1 could not be deleted.").
                                 arg(QDir::toNativeSeparators(f.absoluteFilePath())));
            return false;
        }
    } else {
        QFile file(f.absoluteFilePath());
        file.setPermissions(f.permissions() | QFile::WriteUser);
        if (!file.remove()) {
            if (!errorMessage->isEmpty())
                errorMessage->append(QLatin1Char('\n'));
            errorMessage->append(Tr::tr("The file %1 could not be deleted.").
                                 arg(QDir::toNativeSeparators(f.absoluteFilePath())));
            return false;
        }
    }
    return true;
}

bool removeDirectoryWithContents(const QString &path, QString *errorMessage)
{
    QFileInfo f(path);
    if (f.exists() && !f.isDir()) {
        *errorMessage = Tr::tr("%1 is not a directory.").arg(QDir::toNativeSeparators(path));
        return false;
    }
    return removeFileRecursion(f, errorMessage);
}

/*!
 * Returns the stored link target of the symbolic link \a{filePath}.
 * Unlike QFileInfo::symLinkTarget, this will not make the link target an absolute path.
 */
static QByteArray storedLinkTarget(const QString &filePath)
{
    QByteArray result;

#ifdef Q_OS_UNIX
    const QByteArray nativeFilePath = QFile::encodeName(filePath);
    ssize_t len;
    while (true) {
        struct stat sb{};
        if (lstat(nativeFilePath.constData(), &sb)) {
            qWarning("storedLinkTarget: lstat for %s failed with error code %d",
                     nativeFilePath.constData(), errno);
            return {};
        }

        result.resize(sb.st_size);
        len = readlink(nativeFilePath.constData(), result.data(), sb.st_size + 1);
        if (len < 0) {
            qWarning("storedLinkTarget: readlink for %s failed with error code %d",
                     nativeFilePath.constData(), errno);
            return {};
        }

        if (len < sb.st_size) {
            result.resize(len);
            break;
        }
        if (len == sb.st_size)
            break;
    }
#else
    Q_UNUSED(filePath);
#endif // Q_OS_UNIX

    return result;
}

static bool createSymLink(const QByteArray &path1, const QString &path2)
{
#ifdef Q_OS_UNIX
    const QByteArray newPath = QFile::encodeName(path2);
    unlink(newPath.constData());
    return symlink(path1.constData(), newPath.constData()) == 0;
#else
    Q_UNUSED(path1);
    Q_UNUSED(path2);
    return false;
#endif // Q_OS_UNIX
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
        bool preserveSymLinks, bool copyDirectoryContents, QString *errorMessage)
{
    QFileInfo srcFileInfo(srcFilePath);
    QFileInfo tgtFileInfo(tgtFilePath);
    const QString targetDirPath = tgtFileInfo.absoluteDir().path();
    if (!QDir::root().mkpath(targetDirPath)) {
        *errorMessage = Tr::tr("The directory '%1' could not be created.")
                .arg(QDir::toNativeSeparators(targetDirPath));
        return false;
    }
    if (HostOsInfo::isAnyUnixHost() && preserveSymLinks && srcFileInfo.isSymLink()) {
        // For now, disable symlink preserving copying on Windows.
        // MS did a good job to prevent people from using symlinks - even if they are supported.
        if (!createSymLink(storedLinkTarget(srcFilePath), tgtFilePath)) {
            *errorMessage = Tr::tr("The symlink '%1' could not be created.")
                    .arg(tgtFilePath);
            return false;
        }
    } else if (srcFileInfo.isDir()) {
        if (copyDirectoryContents) {
            QDir sourceDir(srcFilePath);
            const QStringList fileNames = sourceDir.entryList(QDir::Files | QDir::Dirs
                    | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
            for (const QString &fileName : fileNames) {
                const QString newSrcFilePath = srcFilePath + QLatin1Char('/') + fileName;
                const QString newTgtFilePath = tgtFilePath + QLatin1Char('/') + fileName;
                if (!copyFileRecursion(newSrcFilePath, newTgtFilePath, preserveSymLinks,
                                       copyDirectoryContents, errorMessage))
                    return false;
            }
        } else {
            if (tgtFileInfo.exists() && srcFileInfo.lastModified() <= tgtFileInfo.lastModified())
                return true;
            return QDir::root().mkpath(tgtFilePath);
        }
    } else {
        if (tgtFileInfo.exists() && srcFileInfo.lastModified() <= tgtFileInfo.lastModified())
            return true;
        QFile file(srcFilePath);
        QFile targetFile(tgtFilePath);
        if (targetFile.exists()) {
            targetFile.setPermissions(targetFile.permissions() | QFile::WriteUser);
            if (!targetFile.remove()) {
                *errorMessage = Tr::tr("Could not remove file '%1'. %2")
                        .arg(QDir::toNativeSeparators(tgtFilePath), targetFile.errorString());
            }
        }
        if (!file.copy(tgtFilePath)) {
            *errorMessage = Tr::tr("Could not copy file '%1' to '%2'. %3")
                .arg(QDir::toNativeSeparators(srcFilePath), QDir::toNativeSeparators(tgtFilePath),
                     file.errorString());
            return false;
        }
    }
    return true;
}

} // namespace Internal
} // namespace qbs
