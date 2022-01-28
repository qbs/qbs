/****************************************************************************
**
** Copyright (C) 2019 Ivan Komissarov (abbapoh@gmail.com)
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

import qbs.File
import qbs.FileInfo
import qbs.Host
import qbs.ModUtils
import qbs.Utilities
import "../../../modules/cpp/gcc.js" as Gcc

PathProbe {
    // Inputs
    property string compilerFilePath
    property string vcvarsallFilePath
    property stringList enableDefinesByLanguage
    property string preferredArchitecture
    property string winSdkVersion

    // Outputs
    property int versionMajor
    property int versionMinor
    property int versionPatch
    property stringList includePaths
    property string architecture
    property var buildEnv
    property var compilerDefinesByLanguage

    configure: {
        var languages = enableDefinesByLanguage;
        if (!languages || languages.length === 0)
            languages = ["c"];

        var info = languages.contains("c")
                ? Utilities.clangClCompilerInfo(
                      compilerFilePath,
                      preferredArchitecture,
                      vcvarsallFilePath,
                      "c",
                      winSdkVersion)
                : {};
        var infoCpp = languages.contains("cpp")
                ? Utilities.clangClCompilerInfo(
                      compilerFilePath,
                      preferredArchitecture,
                      vcvarsallFilePath,
                      "cpp",
                      winSdkVersion)
                : {};
        found = (!languages.contains("c") ||
                 (!!info && !!info.macros && !!info.buildEnvironment))
             && (!languages.contains("cpp") ||
                 (!!infoCpp && !!infoCpp.macros && !!infoCpp.buildEnvironment));

        compilerDefinesByLanguage = {
            "c": info.macros,
            "cpp": infoCpp.macros,
        };

        var macros = info.macros || infoCpp.macros;

        versionMajor = parseInt(macros["__clang_major__"], 10);
        versionMinor = parseInt(macros["__clang_minor__"], 10);
        versionPatch = parseInt(macros["__clang_patchlevel__"], 10);

        buildEnv = info.buildEnvironment || infoCpp.buildEnvironment;
        // clang-cl is just a wrapper around clang.exe, so the includes *should be* the same
        var clangPath = FileInfo.joinPaths(FileInfo.path(compilerFilePath), "clang.exe");

        var defaultPaths = Gcc.dumpDefaultPaths(buildEnv, clangPath,
                                                [], Host.nullDevice(),
                                                FileInfo.pathListSeparator(), "", "");
        includePaths = defaultPaths.includePaths;
        architecture = ModUtils.guessArchitecture(macros);
    }
}
