/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
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
#ifndef COMMANDLINEFRONTEND_H
#define COMMANDLINEFRONTEND_H

#include "parser/commandlineparser.h"
#include <api/project.h>
#include <api/projectdata.h>

#include <QHash>
#include <QList>
#include <QObject>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace qbs {
class AbstractJob;
class ConsoleProgressObserver;
class ErrorInfo;
class ProcessResult;
class Settings;

class CommandLineFrontend : public QObject
{
    Q_OBJECT
public:
    explicit CommandLineFrontend(const CommandLineParser &parser, Settings *settings,
                                 QObject *parent = 0);
    ~CommandLineFrontend();

    void cancel();

private slots:
    void start();
    void handleCommandDescriptionReport(const QString &highlight, const QString &message);
    void handleJobFinished(bool success, qbs::AbstractJob *job);
    void handleNewTaskStarted(const QString &description, int totalEffort);
    void handleTotalEffortChanged(int totalEffort);
    void handleTaskProgress(int value, qbs::AbstractJob *job);
    void handleProcessResultReport(const qbs::ProcessResult &result);
    void checkCancelStatus();

private:
    typedef QHash<Project, QList<ProductData> > ProductMap;
    ProductMap productsToUse() const;

    bool resolvingMultipleProjects() const;
    bool isResolving() const;
    bool isBuilding() const;
    void handleProjectsResolved();
    void makeClean();
    int runShell();
    void build();
    void generate();
    int runTarget();
    void updateTimestamps();
    void dumpNodesTree();
    void connectBuildJobs();
    void connectBuildJob(AbstractJob *job);
    void connectJob(AbstractJob *job);
    ProductData getTheOneRunnableProduct();
    void install();
    BuildOptions buildOptions(const Project &project) const;
    QString buildDirectory(const QString &profileName) const;

    const CommandLineParser &m_parser;
    Settings * const m_settings;
    QList<AbstractJob *> m_resolveJobs;
    QList<AbstractJob *> m_buildJobs;
    QList<Project> m_projects;

    ConsoleProgressObserver *m_observer;

    enum CancelStatus { CancelStatusNone, CancelStatusRequested, CancelStatusCanceling };
    CancelStatus m_cancelStatus;
    QTimer * const m_cancelTimer;

    int m_buildEffortsNeeded;
    int m_buildEffortsRetrieved;
    int m_totalBuildEffort;
    int m_currentBuildEffort;
    QHash<AbstractJob *, int> m_buildEfforts;
};

} // namespace qbs

#endif // COMMANDLINEFRONTEND_H
