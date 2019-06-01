/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef COMMANDLINEFRONTEND_H
#define COMMANDLINEFRONTEND_H

#include "parser/commandlineparser.h"
#include <api/project.h>
#include <api/projectdata.h>

#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>

#include <memory>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace qbs {
class AbstractJob;
class ConsoleProgressObserver;
class ErrorInfo;
class ProcessResult;
class ProjectGenerator;
class Settings;

class CommandLineFrontend : public QObject
{
    Q_OBJECT
public:
    explicit CommandLineFrontend(const CommandLineParser &parser, Settings *settings,
                                 QObject *parent = nullptr);
    ~CommandLineFrontend() override;

    void cancel();
    void start();

private:
    void handleCommandDescriptionReport(const QString &highlight, const QString &message);
    void handleJobFinished(bool success, qbs::AbstractJob *job);
    void handleNewTaskStarted(const QString &description, int totalEffort);
    void handleTotalEffortChanged(int totalEffort);
    void handleTaskProgress(int value, qbs::AbstractJob *job);
    void handleProcessResultReport(const qbs::ProcessResult &result);
    void checkCancelStatus();

    using ProductMap = QHash<Project, QList<ProductData>>;
    ProductMap productsToUse() const;

    bool resolvingMultipleProjects() const;
    bool isResolving() const;
    bool isBuilding() const;
    void handleProjectsResolved();
    void makeClean();
    int runShell();
    void build();
    void checkGeneratorName();
    void generate();
    int runTarget();
    void updateTimestamps();
    void dumpNodesTree();
    void listProducts();
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

    ConsoleProgressObserver *m_observer = nullptr;

    enum CancelStatus { CancelStatusNone, CancelStatusRequested, CancelStatusCanceling };
    CancelStatus m_cancelStatus = CancelStatus::CancelStatusNone;
    QTimer * const m_cancelTimer;

    int m_buildEffortsNeeded = 0;
    int m_buildEffortsRetrieved = 0;
    int m_totalBuildEffort = 0;
    int m_currentBuildEffort = 0;
    QHash<AbstractJob *, int> m_buildEfforts;
    std::shared_ptr<ProjectGenerator> m_generator;
};

} // namespace qbs

#endif // COMMANDLINEFRONTEND_H
