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

import qbs 1.0
import qbs.ModUtils
import qbs.WindowsUtils

GenericGCC {
    condition: qbs.targetOS.contains("windows") &&
               qbs.toolchain && qbs.toolchain.contains("mingw")
    staticLibraryPrefix: "lib"
    dynamicLibraryPrefix: ""
    executablePrefix: ""
    staticLibrarySuffix: ".a"
    dynamicLibrarySuffix: ".dll"
    executableSuffix: ".exe"
    windowsApiCharacterSet: "unicode"
    platformDefines: base.concat(WindowsUtils.characterSetDefines(windowsApiCharacterSet))
    compilerDefines: ['__GNUC__', 'WIN32', '_WIN32']

    property string windresName: 'windres'
    property path windresPath: { return toolchainPathPrefix + windresName }

    setupBuildEnvironment: {
        var v = new ModUtils.EnvironmentVariable("PATH", ";", true);
        v.prepend(toolchainInstallPath);
        v.set();
    }

    setupRunEnvironment: {
        var v = new ModUtils.EnvironmentVariable("PATH", ";", true);
        v.prepend(toolchainInstallPath);
        v.set();
    }

    FileTagger {
        patterns: ["*.rc"]
        fileTags: ["rc"]
    }

    Rule {
        inputs: ["rc"]
        auxiliaryInputs: ["hpp"]

        Artifact {
            filePath: ".obj/" + qbs.getHash(input.baseDir) + "/" + input.completeBaseName + "_res.o"
            fileTags: ["obj"]
        }

        prepare: {
            var platformDefines = ModUtils.moduleProperty(input, 'platformDefines');
            var defines = ModUtils.moduleProperties(input, 'defines');
            var includePaths = ModUtils.moduleProperties(input, 'includePaths');
            var systemIncludePaths = ModUtils.moduleProperties(input, 'systemIncludePaths');
            var args = [];
            var i;
            for (i in platformDefines) {
                args.push('-D');
                args.push(platformDefines[i]);
            }
            for (i in defines) {
                args.push('-D');
                args.push(defines[i]);
            }
            for (i in includePaths) {
                args.push('-I');
                args.push(includePaths[i]);
            }
            for (i in systemIncludePaths) {
                args.push('-I');
                args.push(systemIncludePaths[i]);
            }

            args = args.concat(['-i', input.filePath, '-o', output.filePath]);
            var cmd = new Command(ModUtils.moduleProperty(product, "windresPath"), args);
            cmd.description = 'compiling ' + input.fileName;
            cmd.highlight = 'compiler';
            return cmd;
        }
    }
}

