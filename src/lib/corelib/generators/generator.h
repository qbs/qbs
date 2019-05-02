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

#ifndef GENERATORPLUGIN_H
#define GENERATORPLUGIN_H

#include "generatordata.h"
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

namespace qbs {

class ProjectGeneratorPrivate;

/*!
 * \class ProjectGenerator
 * \brief The \c ProjectGenerator class is an abstract base class for generators which generate
 * arbitrary output given a resolved Qbs project.
 */
class QBS_EXPORT ProjectGenerator : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ProjectGenerator)
public:
    ~ProjectGenerator() override;

    /*!
     * Returns the name of the generator used to create the external build system files.
     */
    virtual QString generatorName() const = 0;

    ErrorInfo generate(const QList<Project> &projects,
                       const QList<QVariantMap> &buildConfigurations,
                       const InstallOptions &installOptions,
                       const QString &qbsSettingsDir,
                       const Internal::Logger &logger);

    const GeneratableProject project() const;
    QFileInfo qbsExecutableFilePath() const;
    QString qbsSettingsDir() const;

protected:
    ProjectGenerator();

    const Internal::Logger &logger() const;

private:
    virtual void generate() = 0;

    QList<Project> projects() const;
    QList<QVariantMap> buildConfigurations() const;
    QVariantMap buildConfiguration(const Project &project) const;
    QStringList buildConfigurationCommandLine(const Project &project) const;

    ProjectGeneratorPrivate *d;
};

} // namespace qbs

#endif // GENERATORPLUGIN_H
