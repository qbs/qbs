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

#ifndef QBS_EXECUTABLEFINDER_H
#define QBS_EXECUTABLEFINDER_H

#include <language/language.h>
#include <logging/logger.h>

#include <QProcessEnvironment>

namespace qbs {
namespace Internal {

/*!
 * \brief Helper class for finding an executable in the PATH of the build environment.
 */
class ExecutableFinder
{
public:
    ExecutableFinder(const ResolvedProductPtr &product, const QProcessEnvironment &env,
                     const Logger &logger);

    QString findExecutable(const QString &path, const QString &workingDirPath);

private:
    static QStringList m_executableSuffixes;
    QString findBySuffix(const QString &filePath) const;
    bool candidateCheck(const QString &directory, const QString &program,
            QString &fullProgramPath) const;
    QString findInPath(const QString &filePath, const QString &workingDirPath) const;

    QString cachedFilePath(const QString &filePath) const;
    void cacheFilePath(const QString &filePaht, const QString &filePath) const;

    ResolvedProductPtr m_product;
    const QProcessEnvironment &m_environment;
    Logger m_logger;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_EXECUTABLEFINDER_H
