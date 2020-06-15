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

#include "executablefinder.h"

#include "fileinfo.h"
#include "hostosinfo.h"
#include "qttools.h"
#include "stringconstants.h"

#include <logging/categories.h>

#include <QtCore/qdir.h>

namespace qbs {
namespace Internal {

static QStringList populateExecutableSuffixes()
{
    QStringList result;
    result << QString();
    if (HostOsInfo::isWindowsHost()) {
        result << QStringLiteral(".com") << QStringLiteral(".exe")
               << QStringLiteral(".bat") << QStringLiteral(".cmd");
    }
    return result;
}

QStringList ExecutableFinder::m_executableSuffixes = populateExecutableSuffixes();

ExecutableFinder::ExecutableFinder(ResolvedProductPtr product, const QProcessEnvironment &env)
    : m_product(std::move(product))
    , m_environment(env) // QProcessEnvironment doesn't have move-ctor, copy here
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
    qCDebug(lcExec) << "looking for executable by suffix" << fullProgramPath;
    const QString emptyDirectory;
    candidateCheck(emptyDirectory, fullProgramPath, fullProgramPath);
    cacheFilePath(filePath, fullProgramPath);
    return fullProgramPath;

}

bool ExecutableFinder::candidateCheck(const QString &directory, const QString &program,
        QString &fullProgramPath) const
{
    for (const QString &suffix : qAsConst(m_executableSuffixes)) {
        QString candidate = directory + program + suffix;
        qCDebug(lcExec) << "candidate:" << candidate;
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
    qCDebug(lcExec) << "looking for executable in PATH" << fullProgramPath;
    QStringList pathEnv = m_environment.value(StringConstants::pathEnvVar())
            .split(HostOsInfo::pathListSeparator(), QBS_SKIP_EMPTY_PARTS);
    if (HostOsInfo::isWindowsHost())
        pathEnv.prepend(StringConstants::dot());
    for (QString directory : qAsConst(pathEnv)) {
        if (directory == StringConstants::dot())
            directory = workingDirPath;
        if (!directory.isEmpty()) {
            const QChar lastChar = directory.at(directory.size() - 1);
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
