/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef QBS_CREATEPROJECT_H
#define QBS_CREATEPROJECT_H

#include <QtCore/qflags.h>
#include <QtCore/qhash.h>
#include <QtCore/qregexp.h>
#include <QtCore/qstringlist.h>

#include <memory>
#include <vector>

QT_BEGIN_NAMESPACE
class QDir;
class QTextStream;
QT_END_NAMESPACE

enum class ProjectStructure { Flat, Composite };

class ProjectCreator
{
public:
    void run(const QString &topLevelDir, ProjectStructure projectStructure,
             const QStringList &whiteList, const QStringList &blacklist);

private:
    enum ProductFlag { IsApp = 1, NeedsCpp = 2, NeedsQt = 4 };
    Q_DECLARE_FLAGS(ProductFlags, ProductFlag)

    struct Project;
    void setupProject(Project *project);
    void serializeProject(const Project &project);
    void addGroups(QTextStream &stream, const QDir &baseDir, const Project &subProject);
    bool isSourceFile(const QString &fileName);
    ProductFlags getFlags(const Project &project);
    void getFlagsFromFileNames(const Project &project, ProductFlags &flags);
    void getFlagsFromFileContents(const Project &project, ProductFlags &flags);

    using ProjectPtr = std::unique_ptr<Project>;
    struct Project {
        QString dirPath;
        QString dirName;
        QStringList fileNames;
        std::vector<ProjectPtr> subProjects;
    };
    Project m_topLevelProject;
    ProjectStructure m_projectStructure = ProjectStructure::Flat;
    QList<QRegExp> m_whiteList;
    QList<QRegExp> m_blackList;
};

#endif // QBS_CREATEPROJECT_H
