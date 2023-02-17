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
import qbs.Probes
import "cpp.js" as Cpp
import "sdcc.js" as SDCC

CppModule {
    condition: qbs.toolchain && qbs.toolchain.includes("sdcc")

    Probes.BinaryProbe {
        id: compilerPathProbe
        condition: !toolchainInstallPath && !_skipAllChecks
        names: ["sdcc"]
    }

    Probes.SdccProbe {
        id: sdccProbe
        condition: !_skipAllChecks
        compilerFilePath: compilerPath
        enableDefinesByLanguage: enableCompilerDefinesByLanguage
        preferredArchitecture: qbs.architecture
    }

    qbs.architecture: sdccProbe.found ? sdccProbe.architecture : original
    qbs.targetPlatform: "none"

    compilerVersionMajor: sdccProbe.versionMajor
    compilerVersionMinor: sdccProbe.versionMinor
    compilerVersionPatch: sdccProbe.versionPatch
    endianness: sdccProbe.endianness

    compilerDefinesByLanguage: sdccProbe.compilerDefinesByLanguage
    compilerIncludePaths: sdccProbe.includePaths

    toolchainInstallPath: compilerPathProbe.found ? compilerPathProbe.path : undefined

    /* Work-around for QtCreator which expects these properties to exist. */
    property string cCompilerName: compilerName
    property string cxxCompilerName: compilerName

    property string linkerMode: "automatic"

    compilerName: "sdcc" + compilerExtension
    compilerPath: FileInfo.joinPaths(toolchainInstallPath, compilerName)

    assemblerName: toolchainDetails.assemblerName + compilerExtension
    assemblerPath: FileInfo.joinPaths(toolchainInstallPath, assemblerName)

    linkerName: toolchainDetails.linkerName + compilerExtension
    linkerPath: FileInfo.joinPaths(toolchainInstallPath, linkerName)

    property string archiverName: "sdar" + compilerExtension
    property string archiverPath: FileInfo.joinPaths(toolchainInstallPath, archiverName)

    runtimeLibrary: "static"

    staticLibrarySuffix: ".lib"
    executableSuffix: ".ihx"
    objectSuffix: ".rel"

    imageFormat: "ihx"

    enableExceptions: false
    enableRtti: false

    defineFlag: "-D"
    includeFlag: "-I"
    systemIncludeFlag: "-isystem"
    preincludeFlag: "-include"
    libraryDependencyFlag: "-l"
    libraryPathFlag: "-L"
    linkerScriptFlag: "-f"

    toolchainDetails: SDCC.toolchainDetails(qbs)

    knownArchitectures: ["hcs8", "mcs51", "stm8"]

    Rule {
        id: assembler
        inputs: ["asm"]
        outputFileTags: SDCC.extraCompilerOutputTags().concat(
                            Cpp.assemblerOutputTags(generateAssemblerListingFiles))
        outputArtifacts: SDCC.extraCompilerOutputArtifacts(input).concat(
                             Cpp.assemblerOutputArtifacts(input))
        prepare: SDCC.prepareAssembler.apply(SDCC, arguments)
    }

    FileTagger {
        patterns: ["*.s", "*.a51", "*.asm"]
        fileTags: ["asm"]
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c"]
        auxiliaryInputs: ["hpp"]
        outputFileTags: SDCC.extraCompilerOutputTags().concat(
                            Cpp.compilerOutputTags(generateCompilerListingFiles))
        outputArtifacts: SDCC.extraCompilerOutputArtifacts(input).concat(
                             Cpp.compilerOutputArtifacts(input))
        prepare: SDCC.prepareCompiler.apply(SDCC, arguments)
    }

    Rule {
        id: applicationLinker
        multiplex: true
        inputs: ["obj", "linkerscript"]
        inputsFromDependencies: ["staticlibrary"]
        outputFileTags: SDCC.extraApplicationLinkerOutputTags().concat(
                            Cpp.applicationLinkerOutputTags(generateLinkerMapFile))
        outputArtifacts: SDCC.extraApplicationLinkerOutputArtifacts(product).concat(
                             Cpp.applicationLinkerOutputArtifacts(product))
        prepare: SDCC.prepareLinker.apply(SDCC, arguments)
    }

    Rule {
        id: staticLibraryLinker
        multiplex: true
        inputs: ["obj"]
        inputsFromDependencies: ["staticlibrary"]
        outputFileTags: Cpp.staticLibraryLinkerOutputTags()
        outputArtifacts: Cpp.staticLibraryLinkerOutputArtifacts(product)
        prepare: SDCC.prepareArchiver.apply(SDCC, arguments)
    }
}
