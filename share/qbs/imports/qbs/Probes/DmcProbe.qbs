/****************************************************************************
**
** Copyright (C) 2021 Denis Shienkov <denis.shienkov@gmail.com>
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
import qbs.ModUtils
import "../../../modules/cpp/dmc.js" as DMC

PathProbe {
    // Inputs
    property string compilerFilePath
    property stringList enableDefinesByLanguage

    property string _targetPlatform
    property string _targetArchitecture
    property string _targetExtender // Only for DOS 16/32 bit.

    // Outputs
    property string architecture
    property string targetPlatform
    property int versionMajor
    property int versionMinor
    property int versionPatch
    property stringList includePaths
    property var compilerDefinesByLanguage

    configure: {
        compilerDefinesByLanguage = {};

        if (!File.exists(compilerFilePath)) {
            found = false;
            return;
        }

        var languages = enableDefinesByLanguage;
        if (!languages || languages.length === 0)
            languages = ["c"];

        var defaultPathsByLanguage = {};
        for (var i = 0; i < languages.length; ++i) {
            var tag = languages[i];
            compilerDefinesByLanguage[tag] = DMC.dumpMacros(
                        compilerFilePath,
                        _targetPlatform,
                        _targetArchitecture,
                        _targetExtender,
                        tag);
            var paths = DMC.dumpDefaultPaths(compilerFilePath, tag);
            defaultPathsByLanguage[tag] = paths;
        }

        var macros = compilerDefinesByLanguage["c"]
            || compilerDefinesByLanguage["cpp"];

        architecture = ModUtils.guessArchitecture(macros);
        targetPlatform = ModUtils.guessTargetPlatform(macros);

        var defaultPaths = defaultPathsByLanguage["cpp"]
            || defaultPathsByLanguage["c"];

        includePaths = defaultPaths.includePaths;

        var version = DMC.guessVersion(macros);
        if (version) {
            versionMajor = version.major;
            versionMinor = version.minor;
            versionPatch = version.patch;
            found = !!architecture;
        }
   }
}
