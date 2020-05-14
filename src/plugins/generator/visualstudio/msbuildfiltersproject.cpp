/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "msbuildfiltersproject.h"

#include "msbuild/msbuilditemgroup.h"

#include "msbuild/items/msbuildclcompile.h"
#include "msbuild/items/msbuildclinclude.h"
#include "msbuild/items/msbuildfilter.h"
#include "msbuild/items/msbuildnone.h"

#include <tools/set.h>

#include <QtCore/qfileinfo.h>

#include <vector>

namespace qbs {

static QStringList sourceFileExtensions()
{
    return {QStringLiteral("c"), QStringLiteral("C"), QStringLiteral("cpp"),
            QStringLiteral("cxx"), QStringLiteral("c++"), QStringLiteral("cc"),
            QStringLiteral("cs"), QStringLiteral("def"), QStringLiteral("java"),
            QStringLiteral("m"), QStringLiteral("mm")};
}

static QStringList headerFileExtensions()
{
    return {QStringLiteral("h"), QStringLiteral("H"), QStringLiteral("hpp"),
            QStringLiteral("hxx"),  QStringLiteral("h++")};
}

static std::vector<MSBuildFilter *> defaultItemGroupFilters(IMSBuildItemGroup *parent = nullptr)
{
    const auto sourceFilter = new MSBuildFilter(QStringLiteral("Source Files"), sourceFileExtensions(), parent);
    const auto headerFilter = new MSBuildFilter(QStringLiteral("Header Files"), headerFileExtensions(), parent);

    const auto formFilter = new MSBuildFilter(QStringLiteral("Form Files"),
                                              QStringList() << QStringLiteral("ui"), parent);
    const auto resourceFilter = new MSBuildFilter(QStringLiteral("Resource Files"),
                                                  QStringList()
                                                  << QStringLiteral("qrc")
                                                  << QStringLiteral("rc")
                                                  << QStringLiteral("*"), parent);
    resourceFilter->setParseFiles(false);
    const auto generatedFilter = new MSBuildFilter(QStringLiteral("Generated Files"),
                                                   QStringList() << QStringLiteral("moc"), parent);
    generatedFilter->setSourceControlFiles(false);
    const auto translationFilter = new MSBuildFilter(QStringLiteral("Translation Files"),
                                                     QStringList() << QStringLiteral("ts"), parent);
    translationFilter->setParseFiles(false);

    return std::vector<MSBuildFilter *> {
        sourceFilter, headerFilter, formFilter, resourceFilter, generatedFilter, translationFilter
    };
}

static bool matchesFilter(const MSBuildFilter *filter, const QString &filePath)
{
    return filter->extensions().contains(QFileInfo(filePath).completeSuffix());
}

MSBuildFiltersProject::MSBuildFiltersProject(const GeneratableProductData &product,
                                             QObject *parent)
    : MSBuildProject(parent)
{
    // Normally this would be versionInfo.toolsVersion() but for some reason it seems
    // filters projects are always v4.0
    setToolsVersion(QStringLiteral("4.0"));

    const auto itemGroup = new MSBuildItemGroup(this);
    const auto filterOptions = defaultItemGroupFilters();
    for (const auto options : filterOptions) {
        const auto filter = new MSBuildFilter(options->include(), options->extensions(), itemGroup);
        filter->appendProperty(QStringLiteral("ParseFiles"), options->parseFiles());
        filter->appendProperty(QStringLiteral("SourceControlFiles"), options->sourceControlFiles());
    }

    Internal::Set<QString> allFiles;
    const auto productDatas = product.data.values();
    for (const auto &productData : productDatas) {
        for (const auto &groupData : productData.groups())
            if (groupData.isEnabled())
                allFiles.unite(Internal::Set<QString>::fromList(groupData.allFilePaths()));
    }

    MSBuildItemGroup *headerFilesGroup = nullptr;
    MSBuildItemGroup *sourceFilesGroup = nullptr;
    MSBuildItemGroup *filesGroup = nullptr;

    for (const auto &filePath : allFiles) {
        MSBuildFileItem *fileItem = nullptr;

        for (const MSBuildFilter *options : filterOptions) {
            if (matchesFilter(options, filePath)) {
                if (options->include() == QStringLiteral("Header Files")) {
                    if (!headerFilesGroup)
                        headerFilesGroup = new MSBuildItemGroup(this);
                    fileItem = new MSBuildClInclude(headerFilesGroup);
                } else if (options->include() == QStringLiteral("Source Files")) {
                    if (!sourceFilesGroup)
                        sourceFilesGroup = new MSBuildItemGroup(this);
                    fileItem = new MSBuildClCompile(sourceFilesGroup);
                }

                if (fileItem) {
                    fileItem->setFilterName(options->include());
                    break;
                }
            }
        }

        if (!fileItem) {
            if (!filesGroup)
                filesGroup = new MSBuildItemGroup(this);
            fileItem = new MSBuildNone(filesGroup);
        }
        fileItem->setFilePath(filePath);
    }

    qDeleteAll(filterOptions);
}

} // namespace qbs
