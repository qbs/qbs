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

#include "createproject.h"

#include <logging/translator.h>
#include <tools/error.h>
#include <tools/jsliterals.h>
#include <tools/qttools.h>

#include <QtCore/qdebug.h>
#include <QtCore/qdiriterator.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qtextstream.h>

#include <algorithm>

using qbs::ErrorInfo;
using qbs::Internal::Tr;

static const char *indent = "    ";

void ProjectCreator::run(const QString &topLevelDir, ProjectStructure projectStructure,
                         const QStringList &whiteList, const QStringList &blackList)
{
    m_projectStructure = projectStructure;
    for (const QString &s : whiteList)
        m_whiteList.push_back(QRegExp(s, Qt::CaseSensitive, QRegExp::Wildcard));
    for (const QString &s : blackList)
        m_blackList.push_back(QRegExp(s, Qt::CaseSensitive, QRegExp::Wildcard));
    m_topLevelProject.dirPath = topLevelDir;
    setupProject(&m_topLevelProject);
    serializeProject(m_topLevelProject);
}

void ProjectCreator::setupProject(Project *project)
{
    QDirIterator dit(project->dirPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    while (dit.hasNext()) {
        dit.next();
        if (dit.fileInfo().isFile()) {
            if (dit.fileName().endsWith(QLatin1String(".qbs")))
                throw ErrorInfo(Tr::tr("Project already contains qbs files, aborting."));
            if (isSourceFile(dit.fileName()))
                project->fileNames << dit.fileName();
        } else if (dit.fileInfo().isDir()) {
            ProjectPtr subProject(new Project);
            subProject->dirName = dit.fileName();
            subProject->dirPath = dit.filePath();
            setupProject(subProject.get());
            if (!subProject->fileNames.empty() || !subProject->subProjects.empty())
                project->subProjects.push_back(std::move(subProject));
        }
    }
    project->fileNames.sort();
    std::sort(project->subProjects.begin(), project->subProjects.end(),
            [](const ProjectPtr &p1, const ProjectPtr &p2) { return p1->dirName < p2->dirName; });
}

void ProjectCreator::serializeProject(const ProjectCreator::Project &project)
{
    const QString fileName = QFileInfo(project.dirPath).baseName() + QLatin1String(".qbs");
    QFile projectFile(project.dirPath + QLatin1Char('/') + fileName);
    if (!projectFile.open(QIODevice::WriteOnly)) {
        throw ErrorInfo(Tr::tr("Failed to open '%1' for writing: %2")
                        .arg(projectFile.fileName(), projectFile.errorString()));
    }
    QTextStream fileContents(&projectFile);
    fileContents.setCodec("UTF-8");
    fileContents << "import qbs\n\n";
    if (!project.fileNames.empty() || m_projectStructure == ProjectStructure::Flat) {
        fileContents << "Product {\n";
        const ProductFlags productFlags = getFlags(project);
        if (productFlags.testFlag(IsApp)) {
            fileContents << indent << "type: [\"application\"]\n";
        } else {
            fileContents << indent << "type: [\"unknown\"] // E.g. \"application\", "
                            "\"dynamiclibrary\", \"staticlibrary\"\n";
        }
        if (productFlags.testFlag(NeedsQt)) {
            fileContents << indent << "Depends {\n";
            fileContents << indent << indent << "name: \"Qt\"\n";
            fileContents << indent << indent << "submodules: [\"core\"] "
                                                "// Add more here if needed\n";
            fileContents << indent << "}\n";
        } else if (productFlags.testFlag(NeedsCpp)) {
            fileContents << indent << "Depends { name: \"cpp\" }\n";
        }
        fileContents << indent << "files: [\n";
        for (const QString &fileName : qAsConst(project.fileNames))
            fileContents << indent << indent << qbs::toJSLiteral(fileName) << ",\n";
        fileContents << indent << "]\n";
        for (const ProjectPtr &p : project.subProjects)
            addGroups(fileContents, QDir(project.dirPath), *p);
    } else {
        fileContents << "Project {\n";
        fileContents << indent << "references: [\n";
        for (const ProjectPtr &p : project.subProjects) {
            serializeProject(*p);
            fileContents << indent << indent
                         << qbs::toJSLiteral(QFileInfo(p->dirPath).fileName()) << ",\n";
        }
        fileContents << indent << "]\n";
    }
    fileContents << "}\n";
}

void ProjectCreator::addGroups(QTextStream &stream, const QDir &baseDir,
                                 const ProjectCreator::Project &subProject)
{
    stream << indent << "Group {\n";
    stream << indent << indent << "name: "
           << qbs::toJSLiteral(QFileInfo(subProject.dirPath).fileName()) << "\n";
    stream << indent << indent << "prefix: "
           << qbs::toJSLiteral(baseDir.relativeFilePath(subProject.dirPath) + QLatin1Char('/'))
           << '\n';
    stream << indent << indent << "files: [\n";
    for (const QString &fileName : qAsConst(subProject.fileNames))
        stream << indent << indent << indent << qbs::toJSLiteral(fileName) << ",\n";
    stream << indent << indent << "]\n";
    stream << indent << "}\n";
    for (const ProjectPtr &p : subProject.subProjects)
        addGroups(stream, baseDir, *p);
}

bool ProjectCreator::isSourceFile(const QString &fileName)
{
    const auto isMatch = [fileName](const QRegExp &rex) { return rex.exactMatch(fileName); };
    return !std::any_of(m_blackList.cbegin(), m_blackList.cend(), isMatch)
            && (m_whiteList.empty()
                || std::any_of(m_whiteList.cbegin(), m_whiteList.cend(), isMatch));
}

ProjectCreator::ProductFlags ProjectCreator::getFlags(const ProjectCreator::Project &project)
{
    ProductFlags flags;
    getFlagsFromFileNames(project, flags);
    if (flags.testFlag(IsApp) && flags.testFlag(NeedsQt))
        return flags;
    if (!flags.testFlag(NeedsCpp))
        return flags;
    getFlagsFromFileContents(project, flags);
    return flags;
}

void ProjectCreator::getFlagsFromFileNames(const ProjectCreator::Project &project,
                                           ProductFlags &flags)
{
    for (const QString &fileName : qAsConst(project.fileNames)) {
        if (flags.testFlag(IsApp) && flags.testFlag(NeedsQt))
            return;
        const QFileInfo fi(project.dirPath + QLatin1Char('/') + fileName);
        const QString &suffix = fi.suffix();
        if (suffix == QLatin1String("qrc")) {
            flags |= NeedsQt;
            continue;
        }
        if (suffix == QLatin1String("cpp") || suffix == QLatin1String("c")
                || suffix == QLatin1String("m") || suffix == QLatin1String("mm")) {
            flags |= NeedsCpp;
        }
        if (flags.testFlag(NeedsCpp) && fi.completeBaseName() == QLatin1String("main"))
            flags |= IsApp;
    }
    for (const ProjectPtr &p : project.subProjects) {
        getFlagsFromFileNames(*p, flags);
        if (flags.testFlag(IsApp) && flags.testFlag(NeedsQt))
            return;
    }
}

void ProjectCreator::getFlagsFromFileContents(const ProjectCreator::Project &project,
                                              ProductFlags &flags)
{
    for (const QString &fileName : qAsConst(project.fileNames)) {
        QFile f (project.dirPath + QLatin1Char('/') + fileName);
        if (!f.open(QIODevice::ReadOnly)) {
            qDebug() << "Ignoring failure to read" << f.fileName();
            continue;
        }
        while (!f.atEnd()) {
            const QByteArray &line = f.readLine();
            if (line.contains("#include <Q"))
                flags |= NeedsQt;
            if (line.contains("int main"))
                flags |= IsApp;
            if (flags.testFlag(IsApp) && flags.testFlag(NeedsQt))
                return;
        }
    }
    for (const ProjectPtr &p : project.subProjects) {
        getFlagsFromFileContents(*p, flags);
        if (flags.testFlag(IsApp) && flags.testFlag(NeedsQt))
            return;
    }
}
