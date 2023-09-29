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

namespace {

const QStringList & sourceFileExtensions()
{
    static const QStringList EXTENSIONS{
        QStringLiteral("c"),
        QStringLiteral("C"),
        QStringLiteral("cpp"),
        QStringLiteral("cxx"),
        QStringLiteral("c++"),
        QStringLiteral("cc"),
        QStringLiteral("cs"),
        QStringLiteral("def"),
        QStringLiteral("java"),
        QStringLiteral("m"),
        QStringLiteral("mm")};

    return EXTENSIONS;
}

const QStringList & headerFileExtensions()
{
    static const QStringList EXTENSIONS{
        QStringLiteral("h"),
        QStringLiteral("H"),
        QStringLiteral("hpp"),
        QStringLiteral("hxx"),
        QStringLiteral("h++")};

    return EXTENSIONS;
}

struct FilterInfo
{
    QString name;
    QList<QString> extensions;
    bool parseFiles{ true };
    bool sourceControlFiles{ true };
};

const std::vector<FilterInfo> & getDefaultFilterInfo()
{
    static const std::vector<FilterInfo> INFOS {
        {QStringLiteral("Source Files"), sourceFileExtensions()},
        {QStringLiteral("Header Files"), headerFileExtensions()},
        {QStringLiteral("Form Files"), QStringList() << QStringLiteral("ui")},
        {QStringLiteral("Resource Files"), QStringList() << QStringLiteral("qrc") << QStringLiteral("rc") << QStringLiteral("*"), false},
        {QStringLiteral("Generated Files"), QStringList() << QStringLiteral("moc"), true, false},
        {QStringLiteral("Translation Files"), QStringList() << QStringLiteral("ts"), false},
    };

    return INFOS;
}

MSBuildFilter * makeBuildFilter(const FilterInfo &filterInfo,
                               MSBuildItemGroup *itemFiltersGroup)
{
    const auto filter = new MSBuildFilter(filterInfo.name, filterInfo.extensions, itemFiltersGroup);
    filter->appendProperty(QStringLiteral("ParseFiles"), filterInfo.parseFiles);
    filter->appendProperty(QStringLiteral("SourceControlFiles"), filterInfo.sourceControlFiles);
    return filter;
}

bool matchesFilter(const FilterInfo &filterInfo,
                   const QString &filePath)
{
    return filterInfo.extensions.contains(QFileInfo(filePath).completeSuffix());
}

bool isHeaderFile(const QString &filePath)
{
    return headerFileExtensions().contains(QFileInfo(filePath).completeSuffix());
}

bool isSourceFile(const QString &filePath)
{
    return sourceFileExtensions().contains(QFileInfo(filePath).completeSuffix());
}

MSBuildFileItem * makeFileItem(const QString& filePath,
                              MSBuildItemGroup *itemGroup)
{
    if (isHeaderFile(filePath))
        return new MSBuildClInclude(itemGroup);

    if (isSourceFile(filePath))
        return new MSBuildClCompile(itemGroup);

    return new MSBuildNone(itemGroup);
}


class ProductProcessor
{
public:
    using StringSet = Internal::Set<QString>;

    ProductProcessor(MSBuildProject *parent)
        : m_parent(parent)
        , m_itemFiltersGroup(new MSBuildItemGroup(m_parent))
    {
    }

    void operator()(const QList<ProductData> &productDatas)
    {
        for (const auto &productData : productDatas) {
            const auto &productName = productData.name();

            for (const auto &groupData : productData.groups()) {
                if (groupData.name() == productName) {
                    processProductFiles(Internal::rangeTo<StringSet>(groupData.allFilePaths()));
                } else {
                    processGroup(groupData);
                }
            }
        }
    }

    void processProductFiles(const StringSet &files)
    {
        for (const auto &filePath : files) {
            MSBuildFileItem *fileItem = nullptr;

            for (const auto &filterInfo : getDefaultFilterInfo()) {
                if (matchesFilter(filterInfo, filePath)) {
                    makeFilter(filterInfo);

                    if (filterInfo.name == QStringLiteral("Header Files")) {
                        if (!m_headerFilesGroup)
                            m_headerFilesGroup = new MSBuildItemGroup(m_parent);
                        fileItem = new MSBuildClInclude(m_headerFilesGroup);
                    } else if (filterInfo.name == QStringLiteral("Source Files")) {
                        if (!m_sourceFilesGroup)
                            m_sourceFilesGroup = new MSBuildItemGroup(m_parent);
                        fileItem = new MSBuildClCompile(m_sourceFilesGroup);
                    }

                    if (fileItem) {
                        fileItem->setFilterName(filterInfo.name);
                        break;
                    }
                }
            }

            if (!fileItem) {
                if (!m_filesGroup) {
                    m_filesGroup = new MSBuildItemGroup(m_parent);
                }

                fileItem = new MSBuildNone(m_filesGroup);
            }

            fileItem->setFilePath(filePath);
        }
    }

    void processGroup(const GroupData &groupData)
    {
        makeFilter({groupData.name(), QStringList() << QStringLiteral("*")});

        auto *itemGroup = new MSBuildItemGroup(m_parent);
        const auto &files = groupData.allFilePaths();
        for (const auto &filePath : files) {
            auto *fileItem = makeFileItem(filePath, itemGroup);
            fileItem->setFilePath(filePath);
            fileItem->setFilterName(groupData.name());
        }
    }

    void makeFilter(const FilterInfo &filterInfo)
    {
        if (!m_createdFilters.contains(filterInfo.name)) {
            makeBuildFilter(filterInfo, m_itemFiltersGroup);
            m_createdFilters.insert(filterInfo.name);
        }
    }

private:
    MSBuildProject *m_parent = nullptr;
    MSBuildItemGroup *m_itemFiltersGroup = nullptr;
    MSBuildItemGroup *m_headerFilesGroup = nullptr;
    MSBuildItemGroup *m_sourceFilesGroup = nullptr;
    MSBuildItemGroup *m_filesGroup = nullptr;
    QSet<QString> m_createdFilters;
};

} // namespace

MSBuildFiltersProject::MSBuildFiltersProject(const GeneratableProductData &product,
                                             QObject *parent)
    : MSBuildProject(parent)
{
    // Normally this would be versionInfo.toolsVersion() but for some reason it seems
    // filters projects are always v4.0
    setToolsVersion(QStringLiteral("4.0"));

    ProductProcessor(this)(product.data.values());
}

} // namespace qbs
