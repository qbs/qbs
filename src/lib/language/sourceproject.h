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

#include <QList>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QProcessEnvironment;
QT_END_NAMESPACE

namespace qbs {
class BuildOptions;
class RunEnvironment;
class SourceProjectPrivate;
class ProgressObserver;

class SourceProject
{
    Q_DECLARE_TR_FUNCTIONS(SourceProject)
    Q_DISABLE_COPY(SourceProject)
    friend class BuildProject;
public:
    SourceProject();
    ~SourceProject();

    void setProgressObserver(ProgressObserver *observer);
    void setBuildRoot(const QString &directory);

    // These may throw qbs::Error.
    ResolvedProject::ConstPtr setupResolvedProject(const QString &projectFileName,
                                              const QVariantMap &buildConfig);
    void buildProjects(const QList<ResolvedProject::ConstPtr> &projects,
                      const BuildOptions &buildOptions);
    void buildProject(const ResolvedProject::ConstPtr &project, const BuildOptions &buildOptions) {
        buildProjects(QList<ResolvedProject::ConstPtr>() << project, buildOptions);
    }
    void buildProducts(const QList<ResolvedProduct::ConstPtr> &products, const BuildOptions &buildOptions);
    void buildProduct(const ResolvedProduct::ConstPtr &product, const BuildOptions &buildOptions) {
        buildProducts(QList<ResolvedProduct::ConstPtr>() << product, buildOptions);
    }

    RunEnvironment getRunEnvironment(const ResolvedProduct::ConstPtr &product,
                                     const QProcessEnvironment &environment);

    // Empty string if this product is not an application
    QString targetExecutable(const ResolvedProduct::ConstPtr &product);

private:
    SourceProjectPrivate * const d;
};

} // namespace qbs

#endif
