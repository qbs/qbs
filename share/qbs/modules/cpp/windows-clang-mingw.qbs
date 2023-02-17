/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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
import "msvc.js" as MSVC

import "setuprunenv.js" as SetupRunEnv

MingwBaseModule {
    condition: qbs.targetOS.includes("windows") &&
               qbs.toolchain && qbs.toolchain.includes("clang")
    priority: 0

    // llvm-as and llvm-objopy are not shipped with the official binaries on Windows at the
    // moment (8.0). We fall back to using the mingw versions in that case.
    assemblerName: "llvm-as" + compilerExtension
    assemblerPath: {
        if (File.exists(base))
            return base;
        if (qbs.sysroot)
            return FileInfo.joinPaths(qbs.sysroot, "bin", "as" + compilerExtension);
    }
    objcopyName: "llvm-objcopy" + compilerExtension
    objcopyPath: {
        if (File.exists(base))
            return base;
        if (qbs.sysroot)
            return FileInfo.joinPaths(qbs.sysroot, "bin", "objcopy" +  compilerExtension);
    }

    archiverName: "llvm-ar" + compilerExtension

    linkerVariant: "lld"
    targetVendor: "pc"
    targetSystem: "windows"
    targetAbi: "gnu"
    property string rcFilePath: FileInfo.joinPaths(toolchainInstallPath,
                                                   "llvm-rc" + compilerExtension)

    setupBuildEnvironment: {
        if (Host.os().includes("windows") && product.qbs.sysroot) {
            var v = new ModUtils.EnvironmentVariable("PATH", FileInfo.pathListSeparator(), true);
            v.prepend(FileInfo.joinPaths(product.qbs.sysroot, "bin"));
            v.set();
        }
    }

    setupRunEnvironment: {
        if (Host.os().includes("windows") && product.qbs.sysroot) {
            var v = new ModUtils.EnvironmentVariable("PATH", FileInfo.pathListSeparator(), true);
            v.prepend(FileInfo.joinPaths(product.qbs.sysroot, "bin"));
            v.set();
            SetupRunEnv.setupRunEnvironment(product, config);
        }
    }

    Rule {
        inputs: ["rc"]
        auxiliaryInputs: ["hpp"]

        Artifact {
            filePath: FileInfo.joinPaths(Utilities.getHash(input.baseDir),
                                         input.completeBaseName + ".res")
            fileTags: ["obj"]
        }

        prepare: MSVC.createRcCommand(product.cpp.rcFilePath, input, output)
    }
}
