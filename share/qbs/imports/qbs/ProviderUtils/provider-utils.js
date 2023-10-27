/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Copyright (C) 2023 Ivan Komissarov (abbapoh@gmail.com)
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
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

var Utilities = require("qbs.Utilities");

function pkgConfigToModuleName(packageName) {
    return packageName.replace(/\./g, '-');
}

function msvcPrefix() { return "win32-msvc"; }

function isMsvcQt(qtProps) { return qtProps.mkspecName.startsWith(msvcPrefix()); }

function isMinGwQt(qtProps) {
    return qtProps.mkspecName.startsWith("win32-g++") || qtProps.mkspecName.startsWith("mingw");
}

function isDesktopWindowsQt(qtProps) {
    return qtProps.mkspecName.startsWith("win32-") || isMinGwQt(qtProps);
}

function qtNeedsDSuffix(qtProps) {
    return !isMinGwQt(qtProps)
            || Utilities.versionCompare(qtProps.qtVersion, "5.14.0") < 0
            || qtProps.configItems.contains("debug_and_release");
}

function qtIsFramework(modInfo, qtProps) {
    if (!qtProps.frameworkBuild || modInfo.isStaticLibrary)
        return false;
    var modulesNeverBuiltAsFrameworks = [
        "bootstrap", "openglextensions", "platformsupport", "qmldevtools", "harfbuzzng"
    ];

    if (qtProps.qtMajorVersion <= 5) {
        modulesNeverBuiltAsFrameworks.push("uitools"); // is framework since qt6
    }

    return !modulesNeverBuiltAsFrameworks.contains(modInfo.qbsName);
}

function qtLibBaseName(modInfo, libName, debugBuild, qtProps) {
    var name = libName;
    if (qtProps.mkspecName.startsWith("win")) {
        if (debugBuild && qtNeedsDSuffix(qtProps))
            name += 'd';
        if (!modInfo.isStaticLibrary && qtProps.qtMajorVersion < 5)
            name += qtProps.qtMajorVersion;
    }
    if (qtProps.mkspecName.contains("macx")
            || qtProps.mkspecName.contains("ios")
            || qtProps.mkspecName.contains("darwin")) {
        if (!qtIsFramework(modInfo, qtProps)
                && qtProps.buildVariant.contains("debug")
                && (!qtProps.buildVariant.contains("release") || debugBuild)) {
            name += "_debug";
        }
    }
    return name;
}

function qtModuleNameWithoutPrefix(modInfo) {
    if (modInfo.name === "Phonon")
        return "phonon";
    if (!modInfo.modulePrefix && modInfo.name.startsWith("Qt"))
        return modInfo.name.slice(2); // Strip off "Qt".
    if (modInfo.name.startsWith(modInfo.modulePrefix))
        return modInfo.name.slice(modInfo.modulePrefix.length);
    return modInfo.name;
}

function qtLibraryBaseName(modInfo, qtProps, debugBuild) {
    if (modInfo.isPlugin)
        return qtLibBaseName(modInfo, modInfo.name, debugBuild, qtProps);

    // Some modules use a different naming scheme, so it doesn't get boring.
    var libNameBroken = modInfo.name === "Enginio"
            || modInfo.name === "DataVisualization"
            || modInfo.name === "Phonon";

    var libName = "";
    if (!modInfo.isExternal) {
        libName += !modInfo.modulePrefix && !libNameBroken ? "Qt" : modInfo.modulePrefix;
        if (qtProps.qtMajorVersion >= 5 && !qtIsFramework(modInfo, qtProps) && !libNameBroken)
            libName += qtProps.qtMajorVersion;
    }
    libName += qtModuleNameWithoutPrefix(modInfo);
    if (!modInfo.isExternal)
        libName += qtProps.qtLibInfix;
    return qtLibBaseName(modInfo, libName, debugBuild, qtProps);
}

function qtLibNameForLinker(modInfo, qtProps, debugBuild) {
    if (!modInfo.hasLibrary)
        return undefined;
    var libName = qtLibraryBaseName(modInfo, qtProps, debugBuild);
    if (qtProps.mkspecName.contains("msvc"))
        libName += ".lib";
    return libName;
}
