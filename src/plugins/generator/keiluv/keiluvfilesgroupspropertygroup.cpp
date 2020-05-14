/****************************************************************************
**
** Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
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

#include "keiluvfilesgroupspropertygroup.h"
#include "keiluvutils.h"

#include <generators/generatordata.h>

#include <tools/stringconstants.h>

namespace qbs {

class KeiluvFilePropertyGroup final : public gen::xml::PropertyGroup
{
public:
    explicit KeiluvFilePropertyGroup(
            const QString &fullFilePath,
            const QString &baseDirectory)
        : gen::xml::PropertyGroup("File")
    {
        const QFileInfo fileInfo(fullFilePath);
        const auto fileName = fileInfo.fileName();
        const auto fileType = encodeFileType(fileInfo.suffix());
        const auto filePath = QDir::toNativeSeparators(
                    gen::utils::relativeFilePath(
                        baseDirectory,
                        fileInfo.absoluteFilePath()));

        appendChild<gen::xml::Property>(QByteArrayLiteral("FileName"),
                                        fileName);
        appendChild<gen::xml::Property>(QByteArrayLiteral("FileType"),
                                        fileType);
        appendChild<gen::xml::Property>(QByteArrayLiteral("FilePath"),
                                        filePath);
    }

private:
    enum FileType {
        UnknownFileType = 0,
        CSourceFileType = 1,
        AssemblerFileType = 2,
        LibraryFileType = 4,
        TextFileType = 5,
        CppSourceFileType = 8,
    };

    static FileType encodeFileType(const QString &fileSuffix)
    {
        if (fileSuffix.compare(QLatin1String("c"),
                               Qt::CaseInsensitive) == 0) {
            return CSourceFileType;
        } else if (fileSuffix.compare(QLatin1String("cpp"),
                                      Qt::CaseInsensitive) == 0) {
            return CppSourceFileType;
        } else if (fileSuffix.compare(QLatin1String("s"),
                                      Qt::CaseInsensitive) == 0
                   || fileSuffix.compare(QLatin1String("a51"),
                                         Qt::CaseInsensitive) == 0) {
            return AssemblerFileType;
        } else if (fileSuffix.compare(QLatin1String("lib"),
                                      Qt::CaseInsensitive) == 0) {
            return LibraryFileType;
        } else {
            // All header files, text files and include files
            // interpretes as a text file types.
            return TextFileType;
        }
    }
};

class KeiluvFilesPropertyGroup final : public gen::xml::PropertyGroup
{
public:
    explicit KeiluvFilesPropertyGroup(
            const QList<ArtifactData> &sourceArtifacts,
            const QString &baseDirectory)
        : gen::xml::PropertyGroup("Files")
    {
        for (const auto &artifact : sourceArtifacts)
            appendChild<KeiluvFilePropertyGroup>(artifact.filePath(),
                                                 baseDirectory);
    }

    explicit KeiluvFilesPropertyGroup(
            const QStringList &filePaths,
            const QString &baseDirectory)
        : gen::xml::PropertyGroup("Files")
    {
        for (const auto &filePath : filePaths)
            appendChild<KeiluvFilePropertyGroup>(filePath,
                                                 baseDirectory);
    }
};

class KeiluvFileGroupPropertyGroup final : public gen::xml::PropertyGroup
{
public:
    explicit KeiluvFileGroupPropertyGroup(
            const QString &groupName,
            const QList<ArtifactData> &sourceArtifacts,
            const QString &baseDirectory)
        : gen::xml::PropertyGroup("Group")
    {
        appendChild<gen::xml::Property>(QByteArrayLiteral("GroupName"),
                                        groupName);

        appendChild<KeiluvFilesPropertyGroup>(sourceArtifacts,
                                              baseDirectory);
    }

    explicit KeiluvFileGroupPropertyGroup(
            const QString &groupName,
            const QStringList &filePaths,
            const QString &baseDirectory)
        : gen::xml::PropertyGroup("Group")
    {
        appendChild<gen::xml::Property>(QByteArrayLiteral("GroupName"),
                                        groupName);

        appendChild<KeiluvFilesPropertyGroup>(filePaths,
                                              baseDirectory);
    }
};

KeiluvFilesGroupsPropertyGroup::KeiluvFilesGroupsPropertyGroup(
        const Project &qbsProject,
        const ProductData &qbsProduct,
        const std::vector<ProductData> &qbsProductDeps)
    : gen::xml::PropertyGroup(QByteArrayLiteral("Groups"))
{
    const auto baseDirectory = gen::utils::buildRootPath(qbsProject);

    // Build source items.
    for (const auto &group : qbsProduct.groups()) {
        // Ignore disabled groups (e.g. when its condition property is false).
        if (!group.isEnabled())
            continue;
        auto sourceArtifacts = group.sourceArtifacts();
        // Remove the linker script artifacts.
        sourceArtifacts.erase(std::remove_if(sourceArtifacts.begin(),
                                             sourceArtifacts.end(),
                                             [](const auto &artifact){
            const auto tags = artifact.fileTags();
            return tags.contains(QLatin1String("linkerscript"));
        }), sourceArtifacts.end());

        if (sourceArtifacts.isEmpty())
            continue;
        appendChild<KeiluvFileGroupPropertyGroup>(
                    group.name(), sourceArtifacts, baseDirectory);
    }

    // Build local static library items.
    const auto &qbsProps = qbsProduct.moduleProperties();
    const auto staticLibs = KeiluvUtils::staticLibraries(qbsProps);
    if (!staticLibs.isEmpty()) {
        appendChild<KeiluvFileGroupPropertyGroup>(
                    QStringLiteral("Static Libs"), staticLibs, baseDirectory);
    }

    // Build dependency library items.
    const auto deps = KeiluvUtils::dependencies(qbsProductDeps);
    if (!deps.isEmpty()) {
        appendChild<KeiluvFileGroupPropertyGroup>(
                    QStringLiteral("Dependencies"), deps, baseDirectory);
    }
}

} // namespace qbs
