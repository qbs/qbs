/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2015 Jake Petroules.
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

#include "projectgeneratormanager.h"

#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/hostosinfo.h>

#include <QCoreApplication>
#include <QDirIterator>
#include <QLibrary>

#include "generators/clangcompilationdb/clangcompilationdbgenerator.h"
#include "generators/visualstudio/visualstudiogenerator.h"

namespace qbs {

using namespace Internal;

ProjectGeneratorManager::~ProjectGeneratorManager()
{
    foreach (QLibrary * const lib, m_libs) {
        lib->unload();
        delete lib;
    }
}

ProjectGeneratorManager *ProjectGeneratorManager::instance()
{
    static ProjectGeneratorManager generatorPlugin;
    return &generatorPlugin;
}

ProjectGeneratorManager::ProjectGeneratorManager()
{
    QVector<QSharedPointer<ProjectGenerator> > generators;
    generators << QSharedPointer<ClangCompilationDatabaseGenerator>::create();
    generators << qbs::VisualStudioGenerator::createGeneratorList();
    foreach (QSharedPointer<ProjectGenerator> generator, generators) {
        m_generators[generator->generatorName()] = generator;
    }
}

QStringList ProjectGeneratorManager::loadedGeneratorNames()
{
    return instance()->m_generators.keys();
}

QSharedPointer<ProjectGenerator> ProjectGeneratorManager::findGenerator(const QString &generatorName)
{
    return instance()->m_generators.value(generatorName);
}

} // namespace qbs
