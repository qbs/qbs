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
#ifndef QBS_COMMANDLINEPARSER_H
#define QBS_COMMANDLINEPARSER_H

#include "commandtype.h"

#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

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
    bool commandCanResolve() const;
    QString commandDescription() const;
    QString projectFilePath() const;
    QString projectBuildDirectory() const;
    BuildOptions buildOptions(const QString &profile) const;
    CleanOptions cleanOptions(const QString &profile) const;
    GenerateOptions generateOptions() const;
    InstallOptions installOptions(const QString &profile) const;
    bool forceTimestampCheck() const;
    bool forceOutputCheck() const;
    bool dryRun() const;
    bool forceProbesExecution() const;
    bool waitLockBuildGraph() const;
    bool disableFallbackProvider() const;
    bool logTime() const;
    bool withNonDefaultProducts() const;
    bool buildBeforeInstalling() const;
    QStringList runArgs() const;
    QStringList products() const;
    QStringList runEnvConfig() const;
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
