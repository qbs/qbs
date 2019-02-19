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

#include "generatordata.h"
#include <tools/error.h>
#include <tools/set.h>

#include <QtCore/qdir.h>

namespace qbs {

QString GeneratableProductData::name() const
{
    return uniqueValue<QString>(&ProductData::name,
        QStringLiteral("Products with different names per configuration are not "
                      "compatible with this generator. "
                      "Consider using the targetName property instead."));
}

CodeLocation GeneratableProductData::location() const
{
    return uniqueValue<CodeLocation>(&ProductData::location,
        QStringLiteral("GeneratableProductData::location: internal bug; this should not happen."));
}

QStringList GeneratableProductData::dependencies() const
{
    return uniqueValue<QStringList>(&ProductData::dependencies,
        QStringLiteral("Products with different dependency lists per configuration are not "
                      "compatible with this generator."));
}

QStringList GeneratableProductData::type() const
{
    return uniqueValue<QStringList>(&ProductData::type,
        QStringLiteral("Products with different types per configuration are not "
                      "compatible with this generator."));
}

QString GeneratableProductData::buildDirectory() const
{
    return uniqueValue<QString>(&ProductData::buildDirectory,
        QStringLiteral("GeneratableProductData::buildDirectory: "
                      "internal bug; this should not happen."));
}

QString GeneratableProjectData::name() const
{
    return uniqueValue<QString>(&ProjectData::name,
        QStringLiteral("Projects with different names per configuration are not "
                      "compatible with this generator."));
}

CodeLocation GeneratableProjectData::location() const
{
    CodeLocation location;
    QMapIterator<QString, ProjectData> it(data);
    while (it.hasNext()) {
        it.next();
        CodeLocation oldLocation = location;
        location = it.value().location();
        if (oldLocation.isValid() && oldLocation != location)
            throw ErrorInfo(QStringLiteral("Projects with different code locations "
                                          "per configuration are not compatible with this "
                                          "generator."));
    }
    return location;
}

GeneratableProjectData::Id GeneratableProjectData::uniqueName() const
{
    GeneratableProjectData::Id id;
    id.value = name() + QLatin1Char('-') + location().toString();
    return id;
}

QDir GeneratableProject::baseBuildDirectory() const
{
    Internal::Set<QString> baseBuildDirectory;
    QMapIterator<QString, ProjectData> it(data);
    while (it.hasNext()) {
        it.next();
        QDir dir(it.value().buildDirectory());
        dir.cdUp();
        baseBuildDirectory.insert(dir.absolutePath());
    }
    Q_ASSERT(baseBuildDirectory.size() == 1);
    return *baseBuildDirectory.begin();
}

QFileInfo GeneratableProject::filePath() const
{
    Internal::Set<QString> filePath;
    QMapIterator<QString, ProjectData> it(data);
    while (it.hasNext()) {
        it.next();
        filePath.insert(it.value().location().filePath());
    }
    Q_ASSERT(filePath.size() == 1);
    return *filePath.begin();
}

bool GeneratableProject::hasMultipleConfigurations() const
{
    return projects.size() > 1;
}

QStringList GeneratableProject::commandLine() const
{
    QStringList combinedCommandLine;
    QMapIterator<QString, QStringList> it(commandLines);
    while (it.hasNext()) {
        it.next();
        combinedCommandLine << it.value();
    }
    return combinedCommandLine;
}

} // namespace qbs
