/**************************************************************************
**
** This file is part of Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
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
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#ifndef STATUS_H
#define STATUS_H

#include <Qbs/sourceproject.h>
#include <tools/options.h>
#include <tools/fileinfo.h>
#include <tools/logger.h>

#include <QtCore/QFile>
#include <QtCore/QRegExp>
#include <QtCore/QDir>

namespace qbs {

static QList<QRegExp> createIgnoreList(const QString &projectRootPath)
{
    QList<QRegExp> ignoreRegularExpressionList;
    ignoreRegularExpressionList.append(QRegExp(projectRootPath + "/build.*"));
    ignoreRegularExpressionList.append(QRegExp("*.qbs", Qt::CaseSensitive, QRegExp::Wildcard));
    ignoreRegularExpressionList.append(QRegExp("*.qbp", Qt::CaseSensitive, QRegExp::Wildcard));
    ignoreRegularExpressionList.append(QRegExp("*.pro", Qt::CaseSensitive, QRegExp::Wildcard));
    ignoreRegularExpressionList.append(QRegExp("*Makefile", Qt::CaseSensitive, QRegExp::Wildcard));
    ignoreRegularExpressionList.append(QRegExp("*.so*", Qt::CaseSensitive, QRegExp::Wildcard));
    ignoreRegularExpressionList.append(QRegExp("*.o", Qt::CaseSensitive, QRegExp::Wildcard));
    QString ignoreFilePath = projectRootPath + "/.qbsignore";


    QFile ignoreFile(ignoreFilePath);

    if (ignoreFile.open(QFile::ReadOnly)) {
        QList<QByteArray> ignoreTokenList = ignoreFile.readAll().split('\n');
        foreach (const QString &token, ignoreTokenList) {
            if (token.left(1) == "/")
                ignoreRegularExpressionList.append(QRegExp(projectRootPath + token + ".*", Qt::CaseSensitive, QRegExp::RegExp2));
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

static int printStatus(const CommandLineOptions &options, const Qbs::SourceProject &sourceProject)
{
    QString projectDirectory = qbs::FileInfo::path(options.projectFileName());
    int projectDirectoryPathLength = projectDirectory.length();

    QStringList untrackedFilesInProject = allFilesInProject(projectDirectory);
    QStringList missingFiles;
    Qbs::BuildProject buildProject = sourceProject.buildProjects().first();
    foreach (const Qbs::BuildProduct &buildProduct, buildProject.buildProducts()) {
        qbsInfo() << qbs::DontPrintLogLevel << qbs::TextColorBlue << "\nProduct: " << buildProduct.displayName();
        QVector<Qbs::SourceFile> sourceFiles = buildProduct.sourceFiles();
        qSort(sourceFiles.begin(), sourceFiles.end(), Qbs::lessThanSourceFile);
        foreach (const Qbs::SourceFile &sourceFile, sourceFiles) {
            QString fileTags = QStringList(sourceFile.tags.toList()).join(", ");
            qbs::TextColor statusColor = qbs::TextColorDefault;
            if (!qbs::FileInfo(sourceFile.fileName).exists()) {
                statusColor = qbs::TextColorRed;
                missingFiles.append(sourceFile.fileName);
            }
            qbsInfo() << qbs::DontPrintLogLevel<< statusColor <<"  " << sourceFile.fileName.mid(projectDirectoryPathLength + 1) << " [" + fileTags + "]";
            untrackedFilesInProject.removeOne(sourceFile.fileName);
        }
    }

    qbsInfo() << qbs::DontPrintLogLevel << qbs::TextColorDarkBlue << "\nMissing files:";
    foreach (const QString &untrackedFile, missingFiles)
        qbsInfo() << qbs::DontPrintLogLevel<< qbs::TextColorRed <<"  " << untrackedFile.mid(projectDirectoryPathLength + 1);

    qbsInfo() << qbs::DontPrintLogLevel << qbs::TextColorDarkBlue << "\nUntracked files:";
    foreach (const QString &missingFile, untrackedFilesInProject)
        qbsInfo() << qbs::DontPrintLogLevel<< qbs::TextColorCyan <<"  " << missingFile.mid(projectDirectoryPathLength + 1);

    return 0;
}
}

#endif // STATUS_H
