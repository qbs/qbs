/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#include <tools/consolelogger.h>
#include <lib/tools/error.h>

#include <QDir>
#include <QFile>
#include <QTextStream>

#include <cstdio>

using namespace qbs;

ConfigCommandExecutor::ConfigCommandExecutor() : m_settings(Settings::create())
{
}

void ConfigCommandExecutor::execute(const ConfigCommand &command)
{
    if (command.scope == Settings::Local)
        throw Error(tr("Option \"--local\" not implemented yet."));

    switch (command.command) {
    case ConfigCommand::CfgList:
        if (command.scopeSet) {
            if (command.scope == Settings::Local)
                loadLocalSettings(true);
            printSettings(command.scope);
        } else {
            loadLocalSettings(false);
            printSettings(Settings::Local);
            printSettings(Settings::Global);
        }
        break;
    case ConfigCommand::CfgGet:
        if (command.scope == Settings::Local)
            loadLocalSettings(true);
        puts(qPrintable(m_settings->value(command.scope,
                toInternalSeparators(command.varNames.first())).toString()));
        break;
    case ConfigCommand::CfgSet:
        if (command.scope == Settings::Local)
            loadLocalSettings(true);
        m_settings->setValue(command.scope, toInternalSeparators(command.varNames.first()),
                command.varValue);
        break;
    case ConfigCommand::CfgUnset:
        if (command.scope == Settings::Local)
            loadLocalSettings(true);
        foreach (const QString &varName, command.varNames)
            m_settings->remove(command.scope, toInternalSeparators(varName));
        break;
    case ConfigCommand::CfgExport:
        exportGlobalSettings(command.fileName);
        break;
    case ConfigCommand::CfgImport:
        // Display old and new settings, in case import fails or user accidentally nukes everything
        printf("old "); // Will end up as "old global settings:"
        printSettings(Settings::Global);
        importGlobalSettings(command.fileName);
        printf("\nnew ");
        printSettings(Settings::Global);
        break;
    case ConfigCommand::CfgNone:
        qFatal("%s: Impossible command value.", Q_FUNC_INFO);
        break;
    }
}

void ConfigCommandExecutor::loadLocalSettings(bool /* throwExceptionOnFailure */)
{
    // TODO: Implement once we have build-specific settings.
}

void ConfigCommandExecutor::printSettings(Settings::Scope scope)
{
    if (scope == Settings::Global)
        printf("global variables:\n");
    else
        printf("local variables:\n");
    foreach (const QString &key, m_settings->allKeys(scope)) {
        printf("%s: %s\n", qPrintable(toExternalSeparators(key)),
               qPrintable(m_settings->value(scope, key).toString()));
    }
}

QString ConfigCommandExecutor::toInternalSeparators(const QString &variable)
{
    QString transformedVar = variable;
    return transformedVar.replace(QLatin1Char('.'), QLatin1Char('/'));
}

QString ConfigCommandExecutor::toExternalSeparators(const QString &variable)
{
    QString transformedVar = variable;
    return transformedVar.replace(QLatin1Char('/'), QLatin1Char('.'));
}

void ConfigCommandExecutor::exportGlobalSettings(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QFile::Truncate | QFile::WriteOnly | QFile::Text)) {
        throw Error(tr("Could not open file '%1'' for writing: %2")
                .arg(QDir::toNativeSeparators(filename), file.errorString()));
    }
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    foreach (const QString &key, m_settings->allKeys(Settings::Global)) {
        stream << toExternalSeparators(key) << ": "
               << m_settings->value(Settings::Global, key).toString() << endl;
    }
}

void ConfigCommandExecutor::importGlobalSettings(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        throw Error(tr("Could not open file '%1' for reading: %2")
                .arg(QDir::toNativeSeparators(filename), file.errorString()));
    }
    // Remove all current settings
    foreach (const QString &key, m_settings->allKeys(Settings::Global)) {
        m_settings->remove(Settings::Global, key);
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        int colon = line.indexOf(':');
        if (colon >= 0 && !line.startsWith("#")) {
            const QString key = line.left(colon).trimmed();
            const QString value = line.mid(colon + 1).trimmed();
            m_settings->setValue(Settings::Global, toInternalSeparators(key), value);
        }
    }
}
