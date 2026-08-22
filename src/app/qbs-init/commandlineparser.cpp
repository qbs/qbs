/****************************************************************************
**
** Copyright (C) 2026 Ivan Komissarov (abbapoh@gmail.com).
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

#include "commandlineparser.h"

#include <logging/translator.h>
#include <tools/error.h>

#include <QtCore/qcommandlineoption.h>
#include <QtCore/qcommandlineparser.h>
#include <QtCore/qversionnumber.h>

using qbs::ErrorInfo;
using qbs::InitItem;
using qbs::Internal::Tr;

void CommandLineParser::parse(const QStringList &commandLine)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(Tr::tr("Create a new project."));
    const QCommandLineOption languageOpt(
        {QStringLiteral("l"), QStringLiteral("language")},
        Tr::tr("The programming language to use. "
               "Possible values are 'c', 'c++', 'java', and 'objective-c'. "
               "Defaults to 'c++'."),
        QStringLiteral("language"),
        QStringLiteral("c++"));
    const QCommandLineOption versionOpt(
        QStringLiteral("version"),
        Tr::tr("The version of the product to create. Defaults to '1.0.0'."),
        QStringLiteral("version"),
        QStringLiteral("1.0.0"));
    const QCommandLineOption dependsOpt(
        QStringLiteral("depends"),
        Tr::tr("Comma-separated list of module dependencies to add to the product."),
        QStringLiteral("depends"));
    parser.addOption(languageOpt);
    parser.addOption(versionOpt);
    parser.addOption(dependsOpt);
    parser.addHelpOption();
    parser.addPositionalArgument(
        QStringLiteral("project-type"),
        Tr::tr("The type of project to create. "
               "Possible values are 'application' and 'library'."));
    parser.addPositionalArgument(QStringLiteral("name"), Tr::tr("The name of the project."));
    parser.process(commandLine);

    const QStringList positionalArgs = parser.positionalArguments();
    if (positionalArgs.isEmpty())
        throwError(Tr::tr("No project type given."));
    if (positionalArgs.size() == 1)
        throwError(Tr::tr("No project name given."));
    if (positionalArgs.size() > 2)
        throwError(Tr::tr("Unexpected argument '%1'.").arg(positionalArgs.at(2)));

    m_itemType = itemTypeFromName(positionalArgs.at(0));
    m_projectName = positionalArgs.at(1);
    validateProjectName(m_projectName);
    m_language = languageFromName(parser.value(languageOpt));
    m_version = parser.value(versionOpt);
    m_depends = parser.value(dependsOpt).split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (QVersionNumber::fromString(m_version).isNull())
        throwError(Tr::tr("'%1' is not a valid version.").arg(m_version));
}

void CommandLineParser::throwError(const QString &message)
{
    throw ErrorInfo(Tr::tr("Syntax error: %1").arg(message));
}

void CommandLineParser::validateProjectName(const QString &name)
{
    if (name.isEmpty())
        throwError(Tr::tr("Project name must not be empty."));
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\')))
        throwError(Tr::tr("Project name must not contain path separators."));
}

InitItem::Language CommandLineParser::languageFromName(const QString &name)
{
    if (name == QLatin1String("c"))
        return InitItem::Language::C;
    if (name == QLatin1String("c++"))
        return InitItem::Language::Cpp;
    if (name == QLatin1String("java"))
        return InitItem::Language::Java;
    if (name == QLatin1String("objective-c"))
        return InitItem::Language::ObjectiveC;
    throwError(Tr::tr("Unsupported language '%1'.").arg(name));
}

InitItem::ItemType CommandLineParser::itemTypeFromName(const QString &name)
{
    if (name == QLatin1String("application"))
        return InitItem::ItemType::Application;
    if (name == QLatin1String("library"))
        return InitItem::ItemType::Library;
    throwError(Tr::tr("Unsupported project type '%1'.").arg(name));
}
