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
#ifndef QBS_QTMODULEINFO_H
#define QBS_QTMODULEINFO_H

#include <QtCore/qstringlist.h>

namespace qbs {
class QtEnvironment;
class Profile;

namespace Internal {

template<typename T> class Set;

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
    void setupLibraries(const QtEnvironment &qtEnv, Internal::Set<QString> *nonExistingPrlFiles);
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
                        Internal::Set<QString> *nonExistingPrlFiles);
};

QList<QtModuleInfo> allQt4Modules(const QtEnvironment &qtEnvironment);
QList<QtModuleInfo> allQt5Modules(const Profile &profile, const QtEnvironment &qtEnvironment);

} // namespace Internal
} // namespace qbs

#endif // Include guard.
