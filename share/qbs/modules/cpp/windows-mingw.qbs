/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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
import qbs.ModUtils
import qbs.Utilities

import 'cpp.js' as Cpp
import "setuprunenv.js" as SetupRunEnv

MingwBaseModule {
    condition: qbs.targetOS.includes("windows") &&
               qbs.toolchain && qbs.toolchain.includes("mingw")
    priority: 0

    probeEnv: buildEnv

    property string windresName: "windres" + compilerExtension
    property path windresPath: {
        var filePath = toolchainPathPrefix + windresName;
        if (!File.exists(filePath))
            filePath = FileInfo.joinPaths(toolchainInstallPath, windresName);
        return filePath;
    }

    setupBuildEnvironment: {
        var v = new ModUtils.EnvironmentVariable("PATH", FileInfo.pathListSeparator(), true);
        v.prepend(product.cpp.toolchainInstallPath);
        v.set();
    }

    setupRunEnvironment: {
        var v = new ModUtils.EnvironmentVariable("PATH", FileInfo.pathListSeparator(), true);
        v.prepend(product.cpp.toolchainInstallPath);
        v.set();
        SetupRunEnv.setupRunEnvironment(product, config);
    }

    Rule {
        inputs: ["rc"]
        auxiliaryInputs: ["hpp"]
        outputFileTags: Cpp.resourceCompilerOutputTags()
        outputArtifacts: Cpp.resourceCompilerOutputArtifacts(input)
        prepare: {
            var platformDefines = input.cpp.platformDefines;
            var defines = input.cpp.defines;
            var includePaths = input.cpp.includePaths;
            var systemIncludePaths = input.cpp.systemIncludePaths;
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

            args.push("-O", "coff"); // Set COFF format explicitly.
            args = args.concat(['-i', input.filePath, '-o', output.filePath]);
            var cmd = new Command(product.cpp.windresPath, args);
            cmd.description = 'compiling ' + input.fileName;
            cmd.highlight = 'compiler';
            return cmd;
        }
    }
}

