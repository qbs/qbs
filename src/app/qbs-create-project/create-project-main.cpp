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
#include <tools/qttools.h>

#include <QtCore/qcommandlineoption.h>
#include <QtCore/qcommandlineparser.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qstringlist.h>

#include <iostream>

int main(int argc, char *argv[])
{
    using qbs::ErrorInfo;
    using qbs::Internal::Tr;

    QCoreApplication app(argc, argv);
    const QCommandLineOption flatOpt(QStringLiteral("flat"),
            Tr::tr("Do not create nested project files, even if there are subdirectories and "
                   "the top-level directory does not contain any files."));
    const QCommandLineOption whiteListOpt(QStringLiteral("whitelist"),
            Tr::tr("Only consider files whose names match these patterns. The list entries "
                   "can contain wildcards and are separated by commas. By default, all files "
                   "are considered."), QStringLiteral("whitelist"));
    const QCommandLineOption blackListOpt(QStringLiteral("blacklist"),
            Tr::tr("Ignore files whose names match these patterns. The list entries "
                   "can contain wildcards and are separated by commas. By default, no files "
                   "are ignored."), QStringLiteral("blacklist"));
    QCommandLineParser parser;
    parser.setApplicationDescription(Tr::tr("This tool creates a qbs project from an existing "
                                            "source tree.\nNote: The resulting project file(s) "
                                            "will likely require manual editing."));
    parser.addOption(flatOpt);
    parser.addOption(whiteListOpt);
    parser.addOption(blackListOpt);
    parser.addHelpOption();
    parser.process(app);
    const ProjectStructure projectStructure = parser.isSet(flatOpt)
            ? ProjectStructure::Flat : ProjectStructure::Composite;
    const QStringList whiteList = parser.value(whiteListOpt).split(QLatin1Char(','),
                                                                   QBS_SKIP_EMPTY_PARTS);
    const QStringList blackList = parser.value(blackListOpt).split(QLatin1Char(','),
                                                                   QBS_SKIP_EMPTY_PARTS);
    try {
        ProjectCreator().run(QDir::currentPath(), projectStructure, whiteList, blackList);
    } catch (const ErrorInfo &e) {
        std::cerr << qPrintable(Tr::tr("Error creating project: %1").arg(e.toString()))
                  << std::endl;
        return 1;
    }
}
