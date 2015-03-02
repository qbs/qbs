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
    QString libBaseName(const QString &libName, bool debugBuild,
                        const QtEnvironment &qtEnvironment) const;
    QString libNameForLinker(const QtEnvironment &qtEnvironment, bool debugBuild) const;
    void setupLibraries(const QtEnvironment &qtEnv, QSet<QString> *nonExistingPrlFiles);
    bool isFramework(const QtEnvironment &qtEnv) const;

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
    bool mustExist;
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

QList<QtModuleInfo> allQt4Modules(const QtEnvironment &qtEnvironment);
QList<QtModuleInfo> allQt5Modules(const Profile &profile, const QtEnvironment &qtEnvironment);

} // namespace Internal
} // namespace qbs

#endif // Include guard.
