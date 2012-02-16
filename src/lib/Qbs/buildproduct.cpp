/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/


#include "buildproduct.h"

#include <buildgraph/buildgraph.h>
#include <tools/scripttools.h>

namespace Qbs {

SourceFile::SourceFile(const QString &fileNameArg, const QSet<QString> &tagsArg)
    : fileName(fileNameArg), tags(tagsArg)
{
}

QVector<SourceFile> BuildProduct::sourceFiles() const
{
    QVector<SourceFile> artifactList;
    artifactList.reserve(m_internalBuildProduct->rProduct->sources.size());

    if (m_internalBuildProduct) {
        foreach (const qbs::SourceArtifact::Ptr &artifact, m_internalBuildProduct->rProduct->sources)
            artifactList.append(SourceFile(artifact->absoluteFilePath, artifact->fileTags));
    }

    return artifactList;
}

BuildProduct::BuildProduct()
{
}

BuildProduct::~BuildProduct()
{
}

BuildProduct::BuildProduct(const BuildProduct &other)
    : m_internalBuildProduct(other.m_internalBuildProduct)
{
}

BuildProduct &BuildProduct::operator =(const BuildProduct &other)
{
    m_internalBuildProduct = other.m_internalBuildProduct;

    return *this;
}

bool BuildProduct::isValid() const
{
    return m_internalBuildProduct.data();
}

QString BuildProduct::name() const
{
    return m_internalBuildProduct->rProduct->name;
}

static QStringList findProjectIncludePathsRecursive(const QVariantMap &variantMap)
{
    QStringList includePathList;
    QMapIterator<QString, QVariant>  mapIternator(variantMap);

    while (mapIternator.hasNext()) {
        mapIternator.next();
        if (mapIternator.key() == QLatin1String("includePaths")) {
            includePathList.append(mapIternator.value().toStringList());
        } else if (mapIternator.value().userType() == QVariant::Map) {
            includePathList.append(findProjectIncludePathsRecursive(mapIternator.value().toMap()));
        }
    }

    return includePathList;
}

QStringList BuildProduct::projectIncludePaths() const
{
    return findProjectIncludePathsRecursive(m_internalBuildProduct->rProduct->configuration->value());
}

QString BuildProduct::executablePath() const
{
   QString localPath = m_internalBuildProduct->rProduct->name;
   QString destinationDirectory = m_internalBuildProduct->rProduct->destinationDirectory;

   if (!destinationDirectory.isEmpty()) {
       localPath.prepend(destinationDirectory + QLatin1String("/"));
   }

   return QString(m_internalBuildProduct->project->buildGraph()->buildDirectoryRoot()
                  + m_internalBuildProduct->project->resolvedProject()->id
                  + QLatin1String("/")
                  + localPath);
}

bool BuildProduct::isExecutable() const
{
    static const QSet<QString> executableSet(QSet<QString>() << "application" << "applicationbundle");
    return !m_internalBuildProduct->rProduct->fileTags.toSet().intersect(executableSet).isEmpty();
}

BuildProduct::BuildProduct(const QSharedPointer<qbs::BuildProduct> &internalBuildProduct)
    : m_internalBuildProduct(internalBuildProduct)
{
}

QString BuildProduct::displayName() const
{
    return m_internalBuildProduct->rProduct->name;
}

QString BuildProduct::filePath() const
{
     return m_internalBuildProduct->rProduct->qbsFile;
}

} // namespace Qbs
