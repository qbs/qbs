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


#include "buildproject.h"

#include <QSharedData>

#include <buildgraph/buildgraph.h>
#include <tools/scripttools.h>

namespace Qbs {



BuildProject::BuildProject()
{
}

BuildProject::~BuildProject()
{
}

BuildProject::BuildProject(const BuildProject &other)
    : m_internalBuildProject(other.m_internalBuildProject)
{
}

BuildProject &BuildProject::operator =(const BuildProject &other)
{
    m_internalBuildProject = other.m_internalBuildProject;

    return *this;
}

QVector<BuildProduct> BuildProject::buildProducts() const
{
    QVector<BuildProduct> buildProductList;
    buildProductList.reserve(m_internalBuildProject->buildProducts().size());

    foreach (const qbs::BuildProduct::Ptr &internalBuildProduct, m_internalBuildProject->buildProducts())
        buildProductList.append(BuildProduct(internalBuildProduct));

    return buildProductList;
}

BuildProduct BuildProject::buildProductForName(const QString &name) const
{
    foreach (const qbs::BuildProduct::Ptr &internalBuildProduct, m_internalBuildProject->buildProducts()) {
        if (internalBuildProduct->rProduct->name == name)
            return Qbs::BuildProduct(internalBuildProduct);
    }

    return Qbs::BuildProduct();
}

bool BuildProject::hasBuildProductForName(const QString &name) const
{
    foreach (const qbs::BuildProduct::Ptr &internalBuildProduct, m_internalBuildProject->buildProducts()) {
        if (internalBuildProduct->rProduct->name == name)
            return true;
    }

    return false;
}

QString BuildProject::buildDirectory() const
{
    return QString(m_internalBuildProject->buildGraph()->buildDirectoryRoot()
                   + m_internalBuildProject->resolvedProject()->id
                   + QLatin1String("/"));
}

QString BuildProject::displayName() const
{
    return m_internalBuildProject->resolvedProject()->id;
}

void BuildProject::storeBuildGraph()
{
    m_internalBuildProject->store();
}

bool BuildProject::isValid() const
{
    return m_internalBuildProject.data();
}

QString BuildProject::qtInstallPath() const
{
    return qbs::getConfigProperty(m_internalBuildProject->resolvedProject()->configuration->value(),
                                  QStringList() << "qt/core" << "path").toString();
}

BuildProject::BuildProject(const QSharedPointer<qbs::BuildProject> &internalBuildProject)
    : m_internalBuildProject(internalBuildProject)
{
}

qbs::BuildProject::Ptr BuildProject::internalBuildProject() const
{
    return m_internalBuildProject;
}

bool BuildProject::removeBuildDirectory()
{
    bool filesRemoved = false;

#if defined(Q_OS_LINUX)
    QStringList arguments;
    arguments.append("-rf");
    arguments.append(buildDirectory());
    filesRemoved = !QProcess::execute("/bin/rm", arguments);
#elif defined(Q_OS_WIN)
    QString command = QString("rd /s /q \"%1\"").arg(QDir::toNativeSeparators(buildDirectory()));
    QStringList arguments;
    arguments.append("/c");
    arguments.append(command);
    filesRemoved = !QProcess::execute("cmd", arguments);
#endif

    return filesRemoved;
}

} // namespace Qbs
