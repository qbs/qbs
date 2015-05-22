/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
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
#ifndef QBS_COMMANDLINEPARSER_H
#define QBS_COMMANDLINEPARSER_H

#include "commandtype.h"

#include <QStringList>
#include <QVariant>

namespace qbs {
class BuildOptions;
class CleanOptions;
class GenerateOptions;
class InstallOptions;
class Settings;

class CommandLineParser
{
    Q_DISABLE_COPY(CommandLineParser)
public:
    CommandLineParser();
    ~CommandLineParser();

    bool parseCommandLine(const QStringList &args);
    void printHelp() const;

    CommandType command() const;
    QString commandName() const;
    QString commandDescription() const;
    QString projectFilePath() const;
    QString projectBuildDirectory() const;
    BuildOptions buildOptions(const QString &profile) const;
    CleanOptions cleanOptions(const QString &profile) const;
    GenerateOptions generateOptions() const;
    InstallOptions installOptions(const QString &profile) const;
    bool force() const;
    bool forceTimestampCheck() const;
    bool dryRun() const;
    bool logTime() const;
    bool withNonDefaultProducts() const;
    bool buildBeforeInstalling() const;
    QStringList runArgs() const;
    QStringList products() const;
    QList<QVariantMap> buildConfigurations() const;
    bool showProgress() const;
    bool showVersion() const;
    QString settingsDir() const;

private:
    class CommandLineParserPrivate;
    CommandLineParserPrivate *d;
};

} // namespace qbs

#endif // QBS_COMMANDLINEPARSER_H
