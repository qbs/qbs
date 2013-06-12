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

#ifndef QBS_SETUPQT_H
#define QBS_SETUPQT_H

#include <tools/error.h>
#include <QCoreApplication>
#include <QStringList>

namespace qbs {
class Settings;

class QtEnvironment {
public:
    QString installPrefixPath;
    QString libraryPath;
    QString includePath;
    QString binaryPath;
    QString qmlImportPath;
    QString documentationPath;
    QString dataPath;
    QString pluginPath;
    QString qtLibInfix;
    QString qtNameSpace;
    QString mkspecPath;
    QStringList buildVariant;
    QStringList configItems;
    QStringList qtConfigItems;
    QString qtVersion;
    int qtMajorVersion;
    int qtMinorVersion;
    int qtPatchVersion;
    bool frameworkBuild;
    bool staticBuild;
};

class SetupQt
{
    Q_DECLARE_TR_FUNCTIONS(SetupQt)
public:
    static bool isQMakePathValid(const QString &qmakePath);
    static QList<QtEnvironment> fetchEnvironments();
    static QtEnvironment fetchEnvironment(const QString &qmakePath);
    static void saveToQbsSettings(const QString &qtVersionName, const QtEnvironment & qtEnvironment,
                                  Settings *settings);
    static bool checkIfMoreThanOneQtWithTheSameVersion(const QString &qtVersion,
            const QList<QtEnvironment> &qtEnvironments);
};

} // namespace qbs

#endif // QBS_SETUPQT_H
