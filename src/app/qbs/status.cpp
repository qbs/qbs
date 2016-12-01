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

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qstring.h>
#include <QtCore/qregexp.h>

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
