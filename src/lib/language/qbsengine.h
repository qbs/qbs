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

#ifndef QBSINTERFACE_H
#define QBSINTERFACE_H

#include "language.h"

#include "publictypes.h"

#include <QList>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QProcessEnvironment;
QT_END_NAMESPACE

namespace qbs {
class BuildOptions;
class RunEnvironment;
class ProgressObserver;

class QbsEngine
{
    Q_DISABLE_COPY(QbsEngine)
public:
    QbsEngine();
    ~QbsEngine();

    void setProgressObserver(ProgressObserver *observer);
    void setBuildRoot(const QString &directory);

    // All of these may throw qbs::Error.
    Project::Id setupProject(const QString &projectFileName, const QVariantMap &buildConfig);
    void buildProjects(const QList<Project::Id> &projectIds, const BuildOptions &buildOptions);
    void buildProjects(const QList<Project> &projects, const BuildOptions &buildOptions);
    void buildProject(Project::Id projectId, const BuildOptions &buildOptions) {
        buildProjects(QList<Project::Id>() << projectId, buildOptions);
    }
    void buildProducts(const QList<Product> &products, const BuildOptions &buildOptions);
    void buildProduct(const Product &product, const BuildOptions &buildOptions) {
        buildProducts(QList<Product>() << product, buildOptions);
    }

    enum CleanType { CleanupAll, CleanupTemporaries };
    void cleanProjects(const QList<Project::Id> &projectIds, const BuildOptions &buildOptions,
                       CleanType cleanType);
    void cleanProjects(const QList<Project> &projects, const BuildOptions &buildOptions,
                       CleanType cleanType);
    void cleanProject(Project::Id projectId, const BuildOptions &buildOptions, CleanType cleanType)
    {
        cleanProjects(QList<Project::Id>() << projectId, buildOptions, cleanType);
    }
    void cleanProducts(const QList<Product> &products, const BuildOptions &buildOptions,
                       CleanType cleanType);
    void cleanProduct(const Product &product, const BuildOptions &buildOptions, CleanType cleanType)
    {
        cleanProducts(QList<Product>() << product, buildOptions, cleanType);
    }

    Project retrieveProject(Project::Id projectId) const;

    RunEnvironment getRunEnvironment(const Product &product,
                                     const QProcessEnvironment &environment);

    // Empty string if this product is not an application
    QString targetExecutable(const Product &product);

private:
    class QbsEnginePrivate;
    QbsEnginePrivate * const d;
};

} // namespace qbs

#endif
