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

#include "status.h"

#include "../shared/logging/consolelogger.h"

#include <qbs.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QRegExp>

namespace qbs {

static QList<QRegExp> createIgnoreList(const QString &projectRootPath)
{
    QList<QRegExp> ignoreRegularExpressionList;
    ignoreRegularExpressionList.append(QRegExp(projectRootPath + QLatin1String("/build.*")));
    ignoreRegularExpressionList.append(QRegExp(QLatin1String("*.qbs"),
                                               Qt::CaseSensitive, QRegExp::Wildcard));
    ignoreRegularExpressionList.append(QRegExp(QLatin1String("*.pro"),
                                               Qt::CaseSensitive, QRegExp::Wildcard));
    ignoreRegularExpressionList.append(QRegExp(QLatin1String("*Makefile"),
                                               Qt::CaseSensitive, QRegExp::Wildcard));
    ignoreRegularExpressionList.append(QRegExp(QLatin1String("*.so*"),
                                               Qt::CaseSensitive, QRegExp::Wildcard));
    ignoreRegularExpressionList.append(QRegExp(QLatin1String("*.o"),
                                               Qt::CaseSensitive, QRegExp::Wildcard));
    QString ignoreFilePath = projectRootPath + QLatin1String("/.qbsignore");

    QFile ignoreFile(ignoreFilePath);

    if (ignoreFile.open(QFile::ReadOnly)) {
        QList<QByteArray> ignoreTokenList = ignoreFile.readAll().split('\n');
        foreach (const QByteArray &btoken, ignoreTokenList) {
            const QString token = QString::fromLatin1(btoken);
            if (token.left(1) == QLatin1String("/"))
                ignoreRegularExpressionList.append(QRegExp(projectRootPath
                                                           + token + QLatin1String(".*"),
                                                           Qt::CaseSensitive, QRegExp::RegExp2));
            else if (!token.isEmpty())
                ignoreRegularExpressionList.append(QRegExp(token, Qt::CaseSensitive, QRegExp::RegExp2));

        }
    }

    return ignoreRegularExpressionList;
}

static QStringList allFilesInDirectoryRecursive(const QDir &rootDirecory, const QList<QRegExp> &ignoreRegularExpressionList)
{
    QStringList fileList;

    foreach (const QFileInfo &fileInfo, rootDirecory.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        QString absoluteFilePath = fileInfo.absoluteFilePath();
        bool inIgnoreList = false;
        foreach (const QRegExp &ignoreRegularExpression, ignoreRegularExpressionList) {
            if (ignoreRegularExpression.exactMatch(absoluteFilePath)) {
                inIgnoreList = true;
                break;
            }
        }

        if (!inIgnoreList) {
            if (fileInfo.isFile())
                fileList.append(absoluteFilePath);
            else if (fileInfo.isDir())
                fileList.append(allFilesInDirectoryRecursive(QDir(absoluteFilePath), ignoreRegularExpressionList));

        }
    }

    return fileList;
}

static QStringList allFilesInProject(const QString &projectRootPath)
{
    QList<QRegExp> ignoreRegularExpressionList = createIgnoreList(projectRootPath);

    return allFilesInDirectoryRecursive(QDir(projectRootPath), ignoreRegularExpressionList);
}

QStringList allFiles(const ProductData &product)
{
    QStringList files;
    foreach (const GroupData &group, product.groups())
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
    foreach (const ProductData &product, project.allProducts()) {
        qbsInfo() << "\nProduct: " << product.name()
                  << " (" << product.location().filePath() << ":"
                  << product.location().line() << ")";
        foreach (const GroupData &group, product.groups()) {
            qbsInfo() << "  Group: " << group.name()
                      << " (" << group.location().filePath() << ":"
                      << group.location().line() << ")";
            QStringList sourceFiles = group.allFilePaths();
            qSort(sourceFiles);
            foreach (const QString &sourceFile, sourceFiles) {
                if (!QFileInfo(sourceFile).exists())
                    missingFiles.append(sourceFile);
                qbsInfo() << "    " << sourceFile.mid(projectDirectoryPathLength + 1);
                untrackedFilesInProject.removeOne(sourceFile);
            }
        }
    }

    qbsInfo() << "\nMissing files:";
    foreach (const QString &untrackedFile, missingFiles)
        qbsInfo() << untrackedFile.mid(projectDirectoryPathLength + 1);

    qbsInfo() << "\nUntracked files:";
    foreach (const QString &missingFile, untrackedFilesInProject)
        qbsInfo() << missingFile.mid(projectDirectoryPathLength + 1);

    return 0;
}

} // namespace qbs
