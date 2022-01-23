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

#include "status.h"

#include "../shared/logging/consolelogger.h"

#include <qbs.h>
#include <tools/qttools.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qstring.h>

namespace qbs {

static QList<QRegularExpression> createIgnoreList(const QString &projectRootPath)
{
    QList<QRegularExpression> ignoreRegularExpressionList {
        QRegularExpression(QRegularExpression::anchoredPattern(projectRootPath + QLatin1String("/build.*"))),
        QRegularExpression(QRegularExpression::wildcardToRegularExpression(QStringLiteral("*.qbs"))),
        QRegularExpression(QRegularExpression::wildcardToRegularExpression(QStringLiteral("*.pro"))),
        QRegularExpression(QRegularExpression::wildcardToRegularExpression(QStringLiteral("*Makefile"))),
        QRegularExpression(QRegularExpression::wildcardToRegularExpression(QStringLiteral("*.so*"))),
        QRegularExpression(QRegularExpression::wildcardToRegularExpression(QStringLiteral("*.o")))
    };
    QString ignoreFilePath = projectRootPath + QLatin1String("/.qbsignore");

    QFile ignoreFile(ignoreFilePath);

    if (ignoreFile.open(QFile::ReadOnly)) {
        const QList<QByteArray> ignoreTokenList = ignoreFile.readAll().split('\n');
        for (const QByteArray &btoken : ignoreTokenList) {
            const QString token = QString::fromLatin1(btoken);
            if (token.startsWith(QLatin1String("/")))
                ignoreRegularExpressionList.push_back(
                            QRegularExpression(QRegularExpression::anchoredPattern(
                                                   projectRootPath + token + QLatin1String(".*"))));
            else if (!token.isEmpty())
                ignoreRegularExpressionList.push_back(
                            QRegularExpression(QRegularExpression::anchoredPattern(token)));

        }
    }

    return ignoreRegularExpressionList;
}

static QStringList allFilesInDirectoryRecursive(
        const QDir &rootDirecory, const QList<QRegularExpression> &ignoreRegularExpressionList)
{
    QStringList fileList;

    const auto fileInfos = rootDirecory.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &fileInfo : fileInfos) {
        QString absoluteFilePath = fileInfo.absoluteFilePath();
        bool inIgnoreList = false;
        for (const QRegularExpression &ignoreRegularExpression : ignoreRegularExpressionList) {
            if (ignoreRegularExpression.match(absoluteFilePath).hasMatch()) {
                inIgnoreList = true;
                break;
            }
        }

        if (!inIgnoreList) {
            if (fileInfo.isFile())
                fileList.push_back(absoluteFilePath);
            else if (fileInfo.isDir())
                fileList << allFilesInDirectoryRecursive(QDir(absoluteFilePath), ignoreRegularExpressionList);

        }
    }

    return fileList;
}

static QStringList allFilesInProject(const QString &projectRootPath)
{
    QList<QRegularExpression> ignoreRegularExpressionList = createIgnoreList(projectRootPath);

    return allFilesInDirectoryRecursive(QDir(projectRootPath), ignoreRegularExpressionList);
}

QStringList allFiles(const ProductData &product)
{
    QStringList files;
    for (const GroupData &group : product.groups())
        files += group.allFilePaths();
    return files;
}

int printStatus(const ProjectData &project)
{
    const QString projectFilePath = project.location().filePath();
    QString projectDirectory = QFileInfo(projectFilePath).dir().path();
    int projectDirectoryPathLength = projectDirectory.length();

    QStringList untrackedFilesInProject = allFilesInProject(projectDirectory);
    QStringList missingFiles;
    for (const ProductData &product : project.allProducts()) {
        qbsInfo() << "\nProduct: " << product.name()
                  << " (" << product.location().filePath() << ":"
                  << product.location().line() << ")";
        for (const GroupData &group : product.groups()) {
            qbsInfo() << "  Group: " << group.name()
                      << " (" << group.location().filePath() << ":"
                      << group.location().line() << ")";
            const QStringList sourceFiles = Internal::sorted(group.allFilePaths());
            for (const QString &sourceFile : sourceFiles) {
                if (!QFileInfo::exists(sourceFile))
                    missingFiles.push_back(sourceFile);
                qbsInfo() << "    " << sourceFile.mid(projectDirectoryPathLength + 1);
                untrackedFilesInProject.removeOne(sourceFile);
            }
        }
    }

    qbsInfo() << "\nMissing files:";
    for (const QString &untrackedFile : std::as_const(missingFiles))
        qbsInfo() << "    " << untrackedFile.mid(projectDirectoryPathLength + 1);

    qbsInfo() << "\nUntracked files:";
    for (const QString &missingFile : std::as_const(untrackedFilesInProject))
        qbsInfo() << "    " << missingFile.mid(projectDirectoryPathLength + 1);

    return 0;
}

} // namespace qbs
