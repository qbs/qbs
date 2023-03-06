/****************************************************************************
**
** Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
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
import qbs.PathTools
import qbs.Probes
import qbs.Utilities
import "cpp.js" as Cpp
import "iar.js" as IAR

CppModule {
    condition: qbs.toolchain && qbs.toolchain.includes("iar")

    Probes.BinaryProbe {
        id: compilerPathProbe
        condition: !toolchainInstallPath && !_skipAllChecks
        names: ["iccarm"]
    }

    Probes.IarProbe {
        id: iarProbe
        condition: !_skipAllChecks
        compilerFilePath: compilerPath
        enableDefinesByLanguage: enableCompilerDefinesByLanguage
    }

    qbs.architecture: iarProbe.found ? iarProbe.architecture : original
    qbs.targetPlatform: "none"

    compilerVersionMajor: iarProbe.versionMajor
    compilerVersionMinor: iarProbe.versionMinor
    compilerVersionPatch: iarProbe.versionPatch
    endianness: iarProbe.endianness

    compilerDefinesByLanguage: iarProbe.compilerDefinesByLanguage
    compilerIncludePaths: iarProbe.includePaths

    toolchainInstallPath: compilerPathProbe.found ? compilerPathProbe.path : undefined

    /* Work-around for QtCreator which expects these properties to exist. */
    property string cCompilerName: compilerName
    property string cxxCompilerName: compilerName

    compilerName: toolchainDetails.compilerName + compilerExtension
    compilerPath: FileInfo.joinPaths(toolchainInstallPath, compilerName)

    assemblerName: toolchainDetails.assemblerName + compilerExtension
    assemblerPath: FileInfo.joinPaths(toolchainInstallPath, assemblerName)

    linkerName: toolchainDetails.linkerName + compilerExtension
    linkerPath: FileInfo.joinPaths(toolchainInstallPath, linkerName)

    property string archiverName: toolchainDetails.archiverName + compilerExtension
    property string archiverPath: FileInfo.joinPaths(toolchainInstallPath, archiverName)

    runtimeLibrary: "static"

    staticLibrarySuffix: toolchainDetails.staticLibrarySuffix
    executableSuffix: toolchainDetails.executableSuffix
    objectSuffix: toolchainDetails.objectSuffix

    imageFormat: toolchainDetails.imageFormat

    enableExceptions: false
    enableRtti: false

    defineFlag: "-D"
    includeFlag: "-I"
    systemIncludeFlag: "-I"
    preincludeFlag: "--preinclude"
    libraryDependencyFlag: ""
    libraryPathFlag: toolchainDetails.libraryPathFlag
    linkerScriptFlag: toolchainDetails.linkerScriptFlag

    property string linkerSilentFlag: toolchainDetails.linkerSilentFlag
    property string linkerMapFileFlag: toolchainDetails.linkerMapFileFlag
    property string linkerEntryPointFlag: toolchainDetails.linkerEntryPointFlag

    toolchainDetails: IAR.toolchainDetails(qbs)

    knownArchitectures: ["78k", "arm", "avr", "avr32", "cr16",
        "hcs12", "hcs8", "m16c", "m32c", "m68k", "mcs51", "msp430",
        "r32c", "rh850", "riscv", "rl78", "rx", "sh", "stm8", "v850"]

    Rule {
        id: assembler
        inputs: ["asm"]
        outputFileTags: Cpp.assemblerOutputTags(generateAssemblerListingFiles)
        outputArtifacts: Cpp.assemblerOutputArtifacts(input)
        prepare: IAR.prepareAssembler.apply(IAR, arguments)
    }

    FileTagger {
        patterns: ["*.s", "*.s43", "*.s51", "*.s90", "*.msa", "*.asm"]
        fileTags: ["asm"]
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c"]
        auxiliaryInputs: ["hpp"]
        outputFileTags: Cpp.compilerOutputTags(generateCompilerListingFiles)
        outputArtifacts: Cpp.compilerOutputArtifacts(input)
        prepare: IAR.prepareCompiler.apply(IAR, arguments)
    }

    Rule {
        id: applicationLinker
        multiplex: true
        inputs: ["obj", "linkerscript"]
        inputsFromDependencies: ["staticlibrary"]
        outputFileTags: Cpp.applicationLinkerOutputTags(generateLinkerMapFile)
        outputArtifacts: Cpp.applicationLinkerOutputArtifacts(product)
        prepare: IAR.prepareLinker.apply(IAR, arguments)
    }

    Rule {
        id: staticLibraryLinker
        multiplex: true
        inputs: ["obj"]
        inputsFromDependencies: ["staticlibrary"]
        outputFileTags: Cpp.staticLibraryLinkerOutputTags()
        outputArtifacts: Cpp.staticLibraryLinkerOutputArtifacts(product)
        prepare: IAR.prepareArchiver.apply(IAR, arguments)
    }
}
