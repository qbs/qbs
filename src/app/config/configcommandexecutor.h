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
#ifndef CONFIGCOMMANDEXECUTOR_H
#define CONFIGCOMMANDEXECUTOR_H

#include <QCoreApplication>

namespace qbs { class Settings; }

class ConfigCommand;

class ConfigCommandExecutor
{
    Q_DECLARE_TR_FUNCTIONS(ConfigCommandExecutor)
public:
    ConfigCommandExecutor(qbs::Settings *settings);

    void execute(const ConfigCommand &command);

private:
    void setValue(const QString &key, const QString &rawInput);
    void printSettings(const ConfigCommand &command);
    void printOneSetting(const QString &key);
    void exportSettings(const QString &filename);
    void importSettings(const QString &filename);

    qbs::Settings *m_settings;
};

#endif // CONFIGCOMMANDEXECUTOR_H
