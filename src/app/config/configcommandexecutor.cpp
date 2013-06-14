/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/
#include "configcommandexecutor.h"

#include "configcommand.h"
#include "../shared/logging/consolelogger.h"

#include <tools/error.h>
#include <tools/settings.h>

#include <QDir>
#include <QFile>
#include <QTextStream>

#include <cstdio>

using namespace qbs;

ConfigCommandExecutor::ConfigCommandExecutor(Settings *settings) : m_settings(settings)
{
}

void ConfigCommandExecutor::execute(const ConfigCommand &command)
{
    switch (command.command) {
    case ConfigCommand::CfgList:
        printSettings(command);
        break;
    case ConfigCommand::CfgGet:
        puts(qPrintable(m_settings->value(command.varNames.first()).toString()));
        break;
    case ConfigCommand::CfgSet:
        setValue(command.varNames.first(), command.varValue);
        break;
    case ConfigCommand::CfgUnset:
        foreach (const QString &varName, command.varNames)
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
    const QStringList list = rawInput.split(QLatin1Char(','), QString::SkipEmptyParts);
    QVariant actualValue;
    if (list.count() > 1)
        actualValue = list;
    else
        actualValue = rawInput;
    m_settings->setValue(key, actualValue);
}

void ConfigCommandExecutor::printSettings(const ConfigCommand &command)
{
    if (command.varNames.isEmpty()) {
        foreach (const QString &key, m_settings->allKeys())
            printOneSetting(key);
    } else {
        foreach (const QString &parentKey, command.varNames) {
            if (m_settings->value(parentKey).isValid()) { // Key is a leaf.
                printOneSetting(parentKey);
            } else {                                     // Key is a node.
                foreach (const QString &key, m_settings->allKeysWithPrefix(parentKey))
                    printOneSetting(parentKey + QLatin1Char('.') + key);
            }
        }
    }
}

void ConfigCommandExecutor::printOneSetting(const QString &key)
{
    const QStringList value = m_settings->value(key).toStringList();
    printf("%s: %s\n", qPrintable(key), qPrintable(value.join(QLatin1String(","))));
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
    foreach (const QString &key, m_settings->allKeys())
        stream << key << ": " << m_settings->value(key).toString() << endl;
}

void ConfigCommandExecutor::importSettings(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        throw ErrorInfo(tr("Could not open file '%1' for reading: %2")
                .arg(QDir::toNativeSeparators(filename), file.errorString()));
    }
    // Remove all current settings
    foreach (const QString &key, m_settings->allKeys())
        m_settings->remove(key);

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        int colon = line.indexOf(':');
        if (colon >= 0 && !line.startsWith("#")) {
            const QString key = line.left(colon).trimmed();
            const QString value = line.mid(colon + 1).trimmed();
            m_settings->setValue(key, value);
        }
    }
}
