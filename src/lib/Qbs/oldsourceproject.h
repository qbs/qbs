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

#include <QtCore/QFutureInterface>

namespace qbs {

class SourceProject
{
public:
    SourceProject(qbs::Settings::Ptr settings);

    void setSearchPaths(const QStringList &searchPaths);
    void loadPlugins(const QStringList &pluginPaths);
    void loadProject(QFutureInterface<bool> &futureInterface, QString projectFileName, QList<QVariantMap> buildConfigs);

    QList<qbs::BuildProject::Ptr> buildProjects() const;

    QList<qbs::Error> errors() const;

private:
    QStringList m_searchPaths;
    QList<qbs::Configuration::Ptr> m_configurations;

    QSharedPointer<qbs::BuildGraph> m_buildGraph;
    QList<qbs::BuildProject::Ptr> m_buildProjects;

    qbs::Settings::Ptr m_settings;
    QList<qbs::Error> m_errors;
};

} // namespace qbs

#endif
