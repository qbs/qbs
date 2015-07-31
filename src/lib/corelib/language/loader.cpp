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

#include "loader.h"

#include "language.h"
#include "moduleloader.h"
#include "projectresolver.h"
#include "scriptengine.h"

#include <logging/translator.h>
#include <tools/fileinfo.h>
#include <tools/progressobserver.h>
#include <tools/qbsassert.h>
#include <tools/setupprojectparameters.h>

#include <QDir>
#include <QObject>
#include <QTimer>

namespace qbs {
namespace Internal {

Loader::Loader(ScriptEngine *engine, const Logger &logger)
    : m_logger(logger)
    , m_progressObserver(0)
    , m_engine(engine)
{
}

void Loader::setProgressObserver(ProgressObserver *observer)
{
    m_progressObserver = observer;
}

void Loader::setSearchPaths(const QStringList &_searchPaths)
{
    QStringList searchPaths;
    foreach (const QString &searchPath, _searchPaths) {
        if (!FileInfo::exists(searchPath)) {
            m_logger.qbsWarning() << Tr::tr("Search path '%1' does not exist.")
                    .arg(QDir::toNativeSeparators(searchPath));
        } else {
            searchPaths += searchPath;
        }
    }

    m_searchPaths = searchPaths;
}

TopLevelProjectPtr Loader::loadProject(const SetupProjectParameters &parameters)
{
    QBS_CHECK(QFileInfo(parameters.projectFilePath()).isAbsolute());

    m_engine->setEnvironment(parameters.adjustedEnvironment());
    m_engine->clearExceptions();
    m_engine->clearImportsCache();
    m_engine->clearRequestedProperties();

    QTimer cancelationTimer;

    // At this point, we cannot set a sensible total effort, because we know nothing about
    // the project yet. That's why we use a placeholder here, so the user at least
    // sees that an operation is starting. The real total effort will be set later when
    // we have enough information.
    if (m_progressObserver) {
        m_progressObserver->initialize(Tr::tr("Resolving project for configuration %1")
                .arg(TopLevelProject::deriveId(parameters.topLevelProfile(),
                                               parameters.finalBuildConfigurationTree())), 1);
        cancelationTimer.setSingleShot(false);
        QObject::connect(&cancelationTimer, &QTimer::timeout, [this]() {
            QBS_ASSERT(m_progressObserver, return);
            if (m_progressObserver->canceled())
                m_engine->cancel();
        });
        cancelationTimer.start(1000);
    }

    const FileTime resolveTime = FileTime::currentTime();
    ModuleLoader moduleLoader(m_engine, m_logger);
    moduleLoader.setProgressObserver(m_progressObserver);
    moduleLoader.setSearchPaths(m_searchPaths);
    ModuleLoaderResult loadResult = moduleLoader.load(parameters);
    ProjectResolver resolver(&moduleLoader, m_logger);
    resolver.setProgressObserver(m_progressObserver);
    const TopLevelProjectPtr project = resolver.resolve(loadResult, parameters);
    project->lastResolveTime = resolveTime;

    // E.g. if the top-level project is disabled.
    if (m_progressObserver)
        m_progressObserver->setFinished();

    return project;
}

} // namespace Internal
} // namespace qbs
