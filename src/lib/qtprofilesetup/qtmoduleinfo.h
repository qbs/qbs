/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
#ifndef QBS_QTMODULEINFO_H
#define QBS_QTMODULEINFO_H

#include <QSet>
#include <QStringList>

namespace qbs {
class QtEnvironment;
class Profile;

namespace Internal {

class QtModuleInfo
{
public:
    QtModuleInfo();
    QtModuleInfo(const QString &name, const QString &qbsName,
                 const QStringList &deps = QStringList());

    QString moduleNameWithoutPrefix() const;
    QString frameworkHeadersPath(const QtEnvironment &qtEnvironment) const;
    QStringList qt4ModuleIncludePaths(const QtEnvironment &qtEnvironment) const;
    QString libraryBaseName(const QtEnvironment &qtEnvironment, bool debugBuild) const;
    QString libNameForLinker(const QtEnvironment &qtEnvironment, bool debugBuild) const;
    void setupLibraries(const QtEnvironment &qtEnv, QSet<QString> *nonExistingPrlFiles);

    QString modulePrefix; // default is empty and means "Qt".
    QString name; // As in the path to the headers and ".name" in the pri files.
    QString qbsName; // Lower-case version without "qt" prefix.
    QString version;
    QStringList dependencies; // qbs names.
    QStringList includePaths;
    QStringList compilerDefines;
    QStringList staticLibrariesDebug;
    QStringList staticLibrariesRelease;
    QStringList dynamicLibrariesDebug;
    QStringList dynamicLibrariesRelease;
    QStringList linkerFlagsDebug;
    QStringList linkerFlagsRelease;
    QString libFilePathDebug;
    QString libFilePathRelease;
    QStringList frameworksDebug;
    QStringList frameworksRelease;
    QStringList frameworkPathsDebug;
    QStringList frameworkPathsRelease;
    QStringList libraryPaths;
    bool isPrivate;
    bool hasLibrary;
    bool isStaticLibrary;
    bool isPlugin;
    QStringList supportedPluginTypes;

    struct PluginData {
        QString type;
        QString extends;
        QString className;
    } pluginData;

private:
    void setupLibraries(const QtEnvironment &qtEnv, bool debugBuild,
                        QSet<QString> *nonExistingPrlFiles);
};

QString libBaseName(const QString &libName, bool staticLib, bool debugBuild,
                             const QtEnvironment &qtEnvironment);

QList<QtModuleInfo> allQt4Modules(const QtEnvironment &qtEnvironment);
QList<QtModuleInfo> allQt5Modules(const Profile &profile, const QtEnvironment &qtEnvironment);

} // namespace Internal
} // namespace qbs

#endif // Include guard.
