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

#include "executablefinder.h"

#include "fileinfo.h"
#include "hostosinfo.h"

#include <QDir>

namespace qbs {
namespace Internal {

static QStringList populateExecutableSuffixes()
{
    QStringList result;
    result << QString();
    if (HostOsInfo::isWindowsHost()) {
        result << QLatin1String(".com") << QLatin1String(".exe")
               << QLatin1String(".bat") << QLatin1String(".cmd");
    }
    return result;
}

QStringList ExecutableFinder::m_executableSuffixes = populateExecutableSuffixes();

ExecutableFinder::ExecutableFinder(const ResolvedProductPtr &m_product,
        const QProcessEnvironment &env, const Logger &logger)
    : m_product(m_product)
    , m_environment(env)
    , m_logger(logger)
{
}

QString ExecutableFinder::findExecutable(const QString &path, const QString &workingDirPath)
{
    QString filePath = QDir::fromNativeSeparators(path);
    //if (FileInfo::fileName(filePath) == filePath)
    if (!FileInfo::isAbsolute(filePath))
        return findInPath(filePath, workingDirPath);
    else if (HostOsInfo::isWindowsHost())
        return findBySuffix(filePath);
    return filePath;
}

QString ExecutableFinder::findBySuffix(const QString &filePath) const
{
    QString fullProgramPath = cachedFilePath(filePath);
    if (!fullProgramPath.isEmpty())
        return fullProgramPath;

    fullProgramPath = filePath;
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[EXEC] looking for executable by suffix " << fullProgramPath;
    const QString emptyDirectory;
    candidateCheck(emptyDirectory, fullProgramPath, fullProgramPath);
    cacheFilePath(filePath, fullProgramPath);
    return fullProgramPath;

}

bool ExecutableFinder::candidateCheck(const QString &directory, const QString &program,
        QString &fullProgramPath) const
{
    for (int i = 0; i < m_executableSuffixes.count(); ++i) {
        QString candidate = directory + program + m_executableSuffixes.at(i);
        if (m_logger.traceEnabled())
            m_logger.qbsTrace() << "[EXEC] candidate: " << candidate;
        QFileInfo fi(candidate);
        if (fi.isFile() && fi.isExecutable()) {
            fullProgramPath = candidate;
            return true;
        }
    }
    return false;
}

QString ExecutableFinder::findInPath(const QString &filePath, const QString &workingDirPath) const
{
    QString fullProgramPath = cachedFilePath(filePath);
    if (!fullProgramPath.isEmpty())
        return fullProgramPath;

    fullProgramPath = filePath;
    if (m_logger.traceEnabled())
        m_logger.qbsTrace() << "[EXEC] looking for executable in PATH " << fullProgramPath;
    QStringList pathEnv = m_environment.value(QLatin1String("PATH"))
            .split(HostOsInfo::pathListSeparator(), QString::SkipEmptyParts);
    if (HostOsInfo::isWindowsHost())
        pathEnv.prepend(QLatin1String("."));
    for (int i = 0; i < pathEnv.count(); ++i) {
        QString directory = pathEnv.at(i);
        if (directory == QLatin1String("."))
            directory = workingDirPath;
        if (!directory.isEmpty()) {
            const QChar lastChar = directory.at(directory.count() - 1);
            if (lastChar != QLatin1Char('/') && lastChar != QLatin1Char('\\'))
                directory.append(QLatin1Char('/'));
        }
        if (candidateCheck(directory, fullProgramPath, fullProgramPath))
            break;
    }
    cacheFilePath(filePath, fullProgramPath);
    return fullProgramPath;
}

QString ExecutableFinder::cachedFilePath(const QString &filePath) const
{
    return m_product ? m_product->cachedExecutablePath(filePath) : QString();
}

void ExecutableFinder::cacheFilePath(const QString &filePath, const QString &fullFilePath) const
{
    if (m_product)
        m_product->cacheExecutablePath(filePath, fullFilePath);
}

} // namespace Internal
} // namespace qbs
