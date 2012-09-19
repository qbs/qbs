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

#ifndef QBSINTERFACE_H
#define QBSINTERFACE_H

#include <language/language.h>
#include <buildgraph/buildgraph.h>
#include <tools/progressobserver.h>

#include "buildproject.h"
#include "error.h"

#if QT_VERSION >= 0x050000
#    include <QtConcurrent/QFutureInterface>
#else
#    include <QtCore/QFutureInterface>
#endif
#include <QtCore/QExplicitlySharedDataPointer>
#include <QtCore/QSharedPointer>


namespace qbs {
    class BuildProject;
    class Loader;
    class Settings;
}


namespace Qbs {

class SourceProjectPrivate;

class SourceProject : protected qbs::ProgressObserver
{
    Q_DECLARE_TR_FUNCTIONS(SourceProject)
    friend class BuildProject;
public:
    SourceProject();
    ~SourceProject();

    SourceProject(const SourceProject &other);
    SourceProject &operator=(const SourceProject &other);

    void setSettings(const qbs::Settings::Ptr &settings);
    void setSearchPaths(const QStringList &searchPaths);
    void loadPlugins(const QStringList &pluginPaths);
    void loadProjectIde(QFutureInterface<bool> &futureInterface,
                     const QString projectFileName,
                     const QList<QVariantMap> buildConfigs);
    void loadProjectCommandLine(QString projectFileName,
                     QList<QVariantMap> buildConfigs);



    void loadBuildGraphs(const QString projectFileName);

    void storeBuildProjectsBuildGraph();

    void setBuildDirectoryRoot(const QString &buildDirectoryRoot);
    QString buildDirectoryRoot() const;

    QVector<BuildProject> buildProjects() const;
    QList<qbs::BuildProject::Ptr> internalBuildProjects() const;

    QList<Qbs::Error> errors() const;

protected:
    // Implementation of qbs::ProgressObserver
    void setProgressRange(int minimum, int maximum);
    void setProgressValue(int value);
    int progressValue();

private: // functions
    QList<QSharedPointer<qbs::BuildProject> > toInternalBuildProjectList(const QVector<Qbs::BuildProject> &buildProjects) const;
    void loadBuildGraph(const QString &buildGraphPath, const qbs::FileTime &projectFileTimeStamp);

private:
    QExplicitlySharedDataPointer<SourceProjectPrivate> d;
};

}
#endif
