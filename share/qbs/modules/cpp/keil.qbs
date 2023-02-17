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
import qbs.Host
import qbs.Probes
import "cpp.js" as Cpp
import "keil.js" as KEIL

CppModule {
    condition: Host.os().includes("windows") && qbs.toolchain && qbs.toolchain.includes("keil")

    Probes.BinaryProbe {
        id: compilerPathProbe
        condition: !toolchainInstallPath && !_skipAllChecks
        names: ["c51"]
    }

    Probes.KeilProbe {
        id: keilProbe
        condition: !_skipAllChecks
        compilerFilePath: compilerPath
        enableDefinesByLanguage: enableCompilerDefinesByLanguage
    }

    qbs.architecture: keilProbe.found ? keilProbe.architecture : original
    qbs.targetPlatform: "none"

    compilerVersionMajor: keilProbe.versionMajor
    compilerVersionMinor: keilProbe.versionMinor
    compilerVersionPatch: keilProbe.versionPatch
    endianness: keilProbe.endianness

    compilerDefinesByLanguage: keilProbe.compilerDefinesByLanguage
    compilerIncludePaths: keilProbe.includePaths

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

    property string disassemblerName: toolchainDetails.disassemblerName + compilerExtension
    property string disassemblerPath: FileInfo.joinPaths(toolchainInstallPath, disassemblerName)

    runtimeLibrary: "static"

    staticLibrarySuffix: ".lib"
    executableSuffix: toolchainDetails.executableSuffix
    objectSuffix: toolchainDetails.objectSuffix
    linkerMapSuffix: toolchainDetails.linkerMapSuffix

    imageFormat: toolchainDetails.imageFormat

    enableExceptions: false
    enableRtti: false

    defineFlag: "-D"
    includeFlag: "-I"
    systemIncludeFlag: "-I"
    preincludeFlag: KEIL.preincludeFlag(compilerPath)
    libraryDependencyFlag: ""
    libraryPathFlag: "--userlibpath="
    linkerScriptFlag: "--scatter"

    toolchainDetails: KEIL.toolchainDetails(qbs)

    knownArchitectures: ["arm", "c166", "mcs251", "mcs51"]

    Rule {
        id: assembler
        inputs: ["asm"]
        outputFileTags: Cpp.assemblerOutputTags(generateAssemblerListingFiles)
        outputArtifacts: Cpp.assemblerOutputArtifacts(input)
        prepare: KEIL.prepareAssembler.apply(KEIL, arguments)
    }

    FileTagger {
        patterns: ["*.s", "*.a51", ".asm"]
        fileTags: ["asm"]
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c"]
        auxiliaryInputs: ["hpp"]
        outputFileTags: Cpp.compilerOutputTags(generateCompilerListingFiles)
        outputArtifacts: Cpp.compilerOutputArtifacts(input)
        prepare: KEIL.prepareCompiler.apply(KEIL, arguments)
    }

    Rule {
        id: applicationLinker
        multiplex: true
        inputs: ["obj", "linkerscript"]
        inputsFromDependencies: ["staticlibrary"]
        outputFileTags: Cpp.applicationLinkerOutputTags(generateLinkerMapFile)
        outputArtifacts: Cpp.applicationLinkerOutputArtifacts(product)
        prepare: KEIL.prepareLinker.apply(KEIL, arguments)
    }

    Rule {
        id: staticLibraryLinker
        multiplex: true
        inputs: ["obj"]
        inputsFromDependencies: ["staticlibrary"]
        outputFileTags: Cpp.staticLibraryLinkerOutputTags()
        outputArtifacts: Cpp.staticLibraryLinkerOutputArtifacts(product)
        prepare: KEIL.prepareArchiver.apply(KEIL, arguments)
    }
}
