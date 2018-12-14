/****************************************************************************
**
** Copyright (C) 2018 Denis Shienkov <denis.shienkov@gmail.com>
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
import qbs.File
import qbs.FileInfo
import qbs.ModUtils
import qbs.PathTools
import qbs.Probes
import qbs.Utilities
import 'iar.js' as IAR

CppModule {
    condition: qbs.hostOS.contains('windows') &&
               qbs.toolchain && qbs.toolchain.contains('iar')

    Probes.BinaryProbe {
        id: compilerPathProbe
        condition: !toolchainInstallPath && !_skipAllChecks
        names: ["iccarm"]
    }

    Probes.IarProbe {
        id: iarProbe
        condition: !_skipAllChecks
        compilerFilePath: compilerPath
        preferredArchitecture: qbs.architecture
    }

    qbs.architecture: iarProbe.found ? iarProbe.architecture : original

    compilerVersionMajor: iarProbe.versionMajor
    compilerVersionMinor: iarProbe.versionMinor
    compilerVersionPatch: iarProbe.versionPatch
    compilerIncludePaths: iarProbe.includePaths
    endianness: iarProbe.endianness

    compilerDefinesByLanguage: []

    property string toolchainInstallPath: compilerPathProbe.found
        ? compilerPathProbe.path : undefined

    property bool enableCmsis: true
    PropertyOptions {
        name: "enableCmsis"
        description: "enable/disable CMSIS support (enabled by default)"
    }

    property bool enableStaticDestruction;
    PropertyOptions {
        name: "enableStaticDestruction"
        description: "don't emit code to destroy C++ static variables (disabled by default)"
    }

    property string cpu;
    PropertyOptions {
        name: "cpu"
        description: "specify target CPU core (Cortex-M3 is default)"
    }

    property string fpu;
    PropertyOptions {
        name: "fpu"
        description: "specify target FPU coprocessor support " +
            "(default is none, which selects the software floating-point library)"
    }

    compilerName: {
        switch (qbs.architecture) {
        case "arm":
            return "iccarm.exe";
        default:
            return "";
        }
    }
    cCompilerName: compilerName
    cxxCompilerName: compilerName
    compilerPath: FileInfo.joinPaths(toolchainInstallPath, compilerName)

    assemblerName: {
        switch (qbs.architecture) {
        case "arm":
            return "iasmarm.exe";
        default:
            return "";
        }
    }
    assemblerPath: FileInfo.joinPaths(toolchainInstallPath, assemblerName)

    linkerName: {
        switch (qbs.architecture) {
        case "arm":
            return "ilinkarm.exe";
        default:
            return "";
        }
    }
    linkerPath: FileInfo.joinPaths(toolchainInstallPath, linkerName)

    warningLevel: "default"
    runtimeLibrary: "static"
    separateDebugInformation: false
    architecture: qbs.architecture
    staticLibrarySuffix: ".a"
    executableSuffix: ".out"
    imageFormat: "elf"
    enableExceptions: false
    enableRtti: false

    Rule {
        id: assembler
        inputs: ["asm"]

        Artifact {
            fileTags: ['obj']
            filePath: Utilities.getHash(input.baseDir) + "/" + input.fileName + ".o"
        }

        prepare: {
            return IAR.prepareAssembler.apply(IAR, arguments);
        }
    }

    FileTagger {
        patterns: "*.s"
        fileTags: ["asm"]
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c"]
        auxiliaryInputs: ["hpp"]

        Artifact {
            fileTags: ['obj']
            filePath: Utilities.getHash(input.baseDir) + "/" + input.fileName + ".o"
        }

        prepare: {
            return IAR.prepareCompiler.apply(IAR, arguments);
        }
    }

    Rule {
        id: linker
        multiplex: true
        inputs: ["obj", "linkerscript"]

        Artifact {
            fileTags: ['application']
            filePath: FileInfo.joinPaths(
                              product.destinationDirectory,
                              PathTools.applicationFilePath(product))
        }

        prepare: {
            return IAR.prepareLinker.apply(IAR, arguments);
        }
    }

    FileTagger {
        patterns: "*.icf"
        fileTags: ["linkerscript"]
    }

}
