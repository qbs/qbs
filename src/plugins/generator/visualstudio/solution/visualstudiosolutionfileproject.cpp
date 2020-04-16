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

#include "visualstudiosolutionfileproject.h"

#include <QtCore/qfileinfo.h>

namespace qbs {

class VisualStudioSolutionFileProjectPrivate
{
public:
    QString filePath;
};

VisualStudioSolutionFileProject::VisualStudioSolutionFileProject(const QString &filePath,
                                                                 QObject *parent)
    : IVisualStudioSolutionProject(parent)
    , d(new VisualStudioSolutionFileProjectPrivate)
{
    setFilePath(filePath);
}

VisualStudioSolutionFileProject::~VisualStudioSolutionFileProject() = default;

QString VisualStudioSolutionFileProject::name() const
{
    const auto projectName = IVisualStudioSolutionProject::name();
    if (projectName.isEmpty())
        return QFileInfo(filePath()).baseName();
    return projectName;
}

QUuid VisualStudioSolutionFileProject::projectTypeGuid() const
{
    return QStringLiteral("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"); // C++
}

QString VisualStudioSolutionFileProject::filePath() const
{
    return d->filePath;
}

void VisualStudioSolutionFileProject::setFilePath(const QString &filePath)
{
    d->filePath = filePath;
}

} // namespace qbs
