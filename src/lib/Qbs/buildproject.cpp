/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "buildproject.h"

#include <buildgraph/buildgraph.h>
#include <tools/hostosinfo.h>
#include <tools/scripttools.h>

#include <QSharedData>

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
                   + m_internalBuildProject->resolvedProject()->id()
                   + QLatin1String("/"));
}

QString BuildProject::displayName() const
{
    return m_internalBuildProject->resolvedProject()->id();
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
    return qbs::getConfigProperty(m_internalBuildProject->resolvedProject()->configuration(),
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

    if (qbs::HostOsInfo::isLinuxHost()) {
        QStringList arguments;
        arguments.append("-rf");
        arguments.append(buildDirectory());
        filesRemoved = !QProcess::execute("/bin/rm", arguments);
    } else if (qbs::HostOsInfo::isWindowsHost()) {
        QString command = QString("rd /s /q \"%1\"").arg(QDir::toNativeSeparators(buildDirectory()));
        QStringList arguments;
        arguments.append("/c");
        arguments.append(command);
        filesRemoved = !QProcess::execute("cmd", arguments);
    }

    return filesRemoved;
}

} // namespace Qbs
