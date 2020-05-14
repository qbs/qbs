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

#include "clangcompilationdbgenerator.h"

#include <api/projectdata.h>
#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/error.h>
#include <tools/installoptions.h>
#include <tools/shellutils.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>

namespace qbs {
using namespace Internal;

const QString ClangCompilationDatabaseGenerator::DefaultDatabaseFileName =
        QStringLiteral("compile_commands.json");

ClangCompilationDatabaseGenerator::ClangCompilationDatabaseGenerator() = default;

QString ClangCompilationDatabaseGenerator::generatorName() const
{
    return QStringLiteral("clangdb");
}

void ClangCompilationDatabaseGenerator::generate()
{
    const auto projects = project().projects.values();
    for (const Project &theProject : projects) {
        QJsonArray database;
        const ProjectData projectData = theProject.projectData();
        const QString &buildDir = projectData.buildDirectory();

        for (const ProductData &productData : projectData.allProducts()) {
            for (const GroupData &groupData : productData.groups()) {
                const auto sourceArtifacts = groupData.allSourceArtifacts();
                for (const ArtifactData &sourceArtifact : sourceArtifacts) {
                    if (!hasValidInputFileTag(sourceArtifact.fileTags()))
                        continue;

                    const QString filePath = sourceArtifact.filePath();
                    ErrorInfo errorInfo;
                    const RuleCommandList rules = theProject.ruleCommands(productData, filePath,
                                                                       QStringLiteral("obj"),
                                                                       &errorInfo);

                    if (errorInfo.hasError())
                        throw errorInfo;

                    for (const RuleCommand &rule : rules) {
                        if (rule.type() != RuleCommand::ProcessCommandType)
                            continue;
                        database.push_back(createEntry(filePath, buildDir, rule));
                    }
                }
            }
        }

        writeProjectDatabase(QDir(buildDir).filePath(DefaultDatabaseFileName), database);
    }
}

// See http://clang.llvm.org/docs/JSONCompilationDatabase.html
QJsonObject ClangCompilationDatabaseGenerator::createEntry(const QString &filePath,
                                                           const QString &buildDir,
                                                           const RuleCommand &ruleCommand)
{
    QString workDir = ruleCommand.workingDirectory();
    if (workDir.isEmpty())
        workDir = buildDir;

    const QStringList arguments = QStringList() << ruleCommand.executable()
                                                << ruleCommand.arguments();

    const QJsonObject object = {
        { QStringLiteral("directory"), QJsonValue(workDir) },
        { QStringLiteral("arguments"), QJsonArray::fromStringList(arguments) },
        { QStringLiteral("file"), QJsonValue(filePath) }
    };
    return object;
}

void ClangCompilationDatabaseGenerator::writeProjectDatabase(const QString &filePath,
                                                             const QJsonArray &entries)
{
    const QJsonDocument database(entries);
    QFile databaseFile(filePath);

    if (!databaseFile.open(QFile::WriteOnly))
        throw ErrorInfo(Tr::tr("Cannot open '%1' for writing: %2")
                        .arg(filePath)
                        .arg(databaseFile.errorString()));

    if (databaseFile.write(database.toJson()) == -1)
        throw ErrorInfo(Tr::tr("Error while writing '%1': %2")
                        .arg(filePath)
                        .arg(databaseFile.errorString()));
}

bool ClangCompilationDatabaseGenerator::hasValidInputFileTag(const QStringList &fileTags) const
{
    static const QStringList validFileTags = {
        QStringLiteral("c"),
        QStringLiteral("cpp"),
        QStringLiteral("objc"),
        QStringLiteral("objcpp")
    };

    for (const QString &tag : fileTags) {
        if (validFileTags.contains(tag))
            return true;
    }
    return false;
}

} // namespace qbs
