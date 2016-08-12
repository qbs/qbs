/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
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

#ifndef MSBUILDUTILS_H
#define MSBUILDUTILS_H

#include <qbs.h>

namespace qbs {

class MSBuildUtils
{
public:
    static QString _qbsArchitecture(const qbs::Project &project)
    {
        return project.projectConfiguration()
                .value(QStringLiteral("qbs")).toMap()
                .value(QStringLiteral("architecture")).toString();
    }

    static const QString visualStudioArchitectureName(const QString &qbsArch, bool useDisplayName)
    {
        if (qbsArch == QStringLiteral("x86") && useDisplayName)
            return qbsArch;

        // map of qbs architecture names to MSBuild architecture names
        static const QMap<QString, QString> map {
            {QStringLiteral("x86"), QStringLiteral("Win32")},
            {QStringLiteral("x86_64"), QStringLiteral("x64")},
            {QStringLiteral("ia64"), QStringLiteral("Itanium")},
            {QStringLiteral("arm"), QStringLiteral("ARM")},
            {QStringLiteral("arm64"), QStringLiteral("ARM64")}
        };
        return map[qbsArch];
    }

    static QString configurationName(const qbs::Project &project)
    {
        return project.projectConfiguration()
                .value(QStringLiteral("qbs")).toMap()
                .value(QStringLiteral("configurationName")).toString();
    }

    static QString displayPlatform(const qbs::Project &project)
    {
        const auto architecture = _qbsArchitecture(project);
        auto displayPlatform = visualStudioArchitectureName(architecture, true);
        if (displayPlatform.isEmpty())
            displayPlatform = architecture;
        return displayPlatform;
    }

    static QString platform(const qbs::Project &project)
    {
        const auto architecture = _qbsArchitecture(project);
        auto platform = visualStudioArchitectureName(architecture, false);
        if (platform.isEmpty()) {
            qWarning() << "WARNING: Unsupported architecture \""
                       << architecture << "\"; using \"Win32\" platform.";
            platform = QStringLiteral("Win32");
        }

        return platform;
    }

    static QString fullDisplayName(const qbs::Project &project)
    {
        return QStringLiteral("%1|%2")
                .arg(configurationName(project))
                .arg(displayPlatform(project));
    }

    static QString fullName(const qbs::Project &project)
    {
        return QStringLiteral("%1|%2").arg(configurationName(project)).arg(platform(project));
    }

    static QString buildTaskCondition(const Project &buildTask)
    {
        return QStringLiteral("'$(Configuration)|$(Platform)'=='")
                + MSBuildUtils::fullName(buildTask)
                + QStringLiteral("'");
    }
};

} // namespace qbs

#endif // MSBUILDUTILS_H
