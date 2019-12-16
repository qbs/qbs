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
#include "configcommandexecutor.h"

#include "configcommand.h"
#include "../shared/logging/consolelogger.h"

#include <tools/error.h>
#include <tools/qttools.h>
#include <tools/settingsrepresentation.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qtextstream.h>

#include <cstdio>

using namespace qbs;

ConfigCommandExecutor::ConfigCommandExecutor(Settings *settings, Settings::Scopes scope)
    : m_settings(settings), m_scope(scope)
{
    if (m_scope == qbs::Settings::SystemScope)
        m_settings->setScopeForWriting(qbs::Settings::SystemScope);
}

void ConfigCommandExecutor::execute(const ConfigCommand &command)
{
    switch (command.command) {
    case ConfigCommand::CfgList:
        printSettings(command);
        break;
    case ConfigCommand::CfgSet:
        setValue(command.varNames.front(), command.varValue);
        break;
    case ConfigCommand::CfgUnset:
        for (const QString &varName : command.varNames)
            m_settings->remove(varName);
        break;
    case ConfigCommand::CfgExport:
        exportSettings(command.fileName);
        break;
    case ConfigCommand::CfgImport:
        // Display old and new settings, in case import fails or user accidentally nukes everything
        printf("old "); // Will end up as "old settings:"
        printSettings(command);
        importSettings(command.fileName);
        printf("\nnew ");
        printSettings(command);
        break;
    case ConfigCommand::CfgNone:
        qFatal("%s: Impossible command value.", Q_FUNC_INFO);
        break;
    }
}

void ConfigCommandExecutor::setValue(const QString &key, const QString &rawInput)
{
    m_settings->setValue(key, representationToSettingsValue(rawInput));
}

void ConfigCommandExecutor::printSettings(const ConfigCommand &command)
{
    if (command.varNames.empty()) {
        const auto keys = m_settings->allKeys(m_scope);
        for (const QString &key : keys)
            printOneSetting(key);
    } else {
        for (const QString &parentKey : command.varNames) {
            if (m_settings->value(parentKey, m_scope).isValid()) { // Key is a leaf.
                printOneSetting(parentKey);
            } else {                                     // Key is a node.
                const auto keys = m_settings->allKeysWithPrefix(parentKey, m_scope);
                for (const QString &key : keys)
                    printOneSetting(parentKey + QLatin1Char('.') + key);
            }
        }
    }
}

void ConfigCommandExecutor::printOneSetting(const QString &key)
{
    printf("%s: %s\n", qPrintable(key),
           qPrintable(qbs::settingsValueToRepresentation(m_settings->value(key, m_scope))));
 }

void ConfigCommandExecutor::exportSettings(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QFile::Truncate | QFile::WriteOnly | QFile::Text)) {
        throw ErrorInfo(tr("Could not open file '%1' for writing: %2")
                .arg(QDir::toNativeSeparators(filename), file.errorString()));
    }
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    const auto keys = m_settings->allKeys(m_scope);
    for (const QString &key : keys)
        stream << key << ": " << qbs::settingsValueToRepresentation(m_settings->value(key, m_scope))
               << Qt::endl;
}

void ConfigCommandExecutor::importSettings(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        throw ErrorInfo(tr("Could not open file '%1' for reading: %2")
                .arg(QDir::toNativeSeparators(filename), file.errorString()));
    }
    // Remove all current settings
    const auto keys = m_settings->allKeys(m_scope);
    for (const QString &key : keys)
        m_settings->remove(key);

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        int colon = line.indexOf(QLatin1Char(':'));
        if (colon >= 0 && !line.startsWith(QLatin1Char('#'))) {
            const QString key = line.left(colon).trimmed();
            const QString value = line.mid(colon + 1).trimmed();
            m_settings->setValue(key, representationToSettingsValue(value));
        }
    }
}
