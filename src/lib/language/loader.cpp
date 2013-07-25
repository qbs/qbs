/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#include "loader.h"

#include "builtindeclarations.h"
#include "item.h"
#include "moduleloader.h"
#include "projectresolver.h"
#include <logging/translator.h>
#include <tools/fileinfo.h>
#include <tools/progressobserver.h>
#include <tools/qbsassert.h>
#include <tools/setupprojectparameters.h>

#include <QDir>

namespace qbs {
namespace Internal {

Loader::Loader(ScriptEngine *engine, const Logger &logger)
    : m_logger(logger)
    , m_progressObserver(0)
    , m_builtins(new BuiltinDeclarations)
    , m_moduleLoader(new ModuleLoader(engine, m_builtins, logger))
    , m_projectResolver(new ProjectResolver(m_moduleLoader, m_builtins, logger))
    , m_engine(engine)
{
}

Loader::~Loader()
{
    delete m_projectResolver;
    delete m_moduleLoader;
    delete m_builtins;
}

void Loader::setProgressObserver(ProgressObserver *observer)
{
    m_progressObserver = observer;
    m_moduleLoader->setProgressObserver(observer);
    m_projectResolver->setProgressObserver(observer);
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

    m_moduleLoader->setSearchPaths(searchPaths);
}

TopLevelProjectPtr Loader::loadProject(const SetupProjectParameters &parameters)
{
    QBS_CHECK(QFileInfo(parameters.projectFilePath()).isAbsolute());

    m_engine->setEnvironment(parameters.environment());
    m_engine->clearExceptions();

    // At this point, we cannot set a sensible total effort, because we know nothing about
    // the project yet. That's why we use a placeholder here, so the user at least
    // sees that an operation is starting. The real total effort will be set later when
    // we have enough information.
    if (m_progressObserver) {
        m_progressObserver->initialize(Tr::tr("Resolving project for configuration %1")
                .arg(TopLevelProject::deriveId(parameters.buildConfigurationTree())), 1);
    }

    ModuleLoaderResult loadResult
            = m_moduleLoader->load(parameters.projectFilePath(),
                                   parameters.overriddenValuesTree(),
                                   parameters.buildConfigurationTree(),
                                   true);
    const TopLevelProjectPtr project = m_projectResolver->resolve(loadResult,
            parameters.buildRoot(), parameters.overriddenValuesTree(),
            parameters.buildConfigurationTree());

    // E.g. if the top-level project is disabled.
    if (m_progressObserver)
        m_progressObserver->setFinished();

    return project;
}

QByteArray Loader::qmlTypeInfo()
{
    return m_builtins->qmlTypeInfo();
}

} // namespace Internal
} // namespace qbs
