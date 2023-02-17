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
import qbs.FileInfo
import qbs.PathTools
import qbs.Probes
import qbs.Utilities
import "cosmic.js" as COSMIC
import "cpp.js" as Cpp

CppModule {
    condition: qbs.toolchain && qbs.toolchain.includes("cosmic")

    Probes.BinaryProbe {
        id: compilerPathProbe
        condition: !toolchainInstallPath && !_skipAllChecks
        names: ["cxcorm"]
    }

    Probes.CosmicProbe {
        id: cosmicProbe
        condition: !_skipAllChecks
        compilerFilePath: compilerPath
        enableDefinesByLanguage: enableCompilerDefinesByLanguage
    }

    qbs.architecture: cosmicProbe.found ? cosmicProbe.architecture : original
    qbs.targetPlatform: "none"

    compilerVersionMajor: cosmicProbe.versionMajor
    compilerVersionMinor: cosmicProbe.versionMinor
    compilerVersionPatch: cosmicProbe.versionPatch
    endianness: cosmicProbe.endianness

    compilerDefinesByLanguage: cosmicProbe.compilerDefinesByLanguage
    compilerIncludePaths: cosmicProbe.includePaths

    toolchainInstallPath: compilerPathProbe.found ? compilerPathProbe.path : undefined

    /* Work-around for QtCreator which expects these properties to exist. */
    property string cCompilerName: compilerName
    property string cxxCompilerName: compilerName

    compilerName: toolchainDetails.compilerName + compilerExtension
    compilerPath: FileInfo.joinPaths(toolchainInstallPath, compilerName)

    assemblerName: toolchainDetails.assemblerName + compilerExtension
    assemblerPath: FileInfo.joinPaths(toolchainInstallPath, assemblerName)

    linkerName: "clnk" + compilerExtension
    linkerPath: FileInfo.joinPaths(toolchainInstallPath, linkerName)

    property string archiverName: "clib" + compilerExtension
    property string archiverPath: FileInfo.joinPaths(toolchainInstallPath, archiverName)

    runtimeLibrary: "static"

    staticLibrarySuffix: toolchainDetails.staticLibrarySuffix
    executableSuffix: toolchainDetails.executableSuffix

    imageFormat: "cosmic"

    enableExceptions: false
    enableRtti: false

    defineFlag: "-d"
    includeFlag: "-i"
    systemIncludeFlag: "-i"
    preincludeFlag: "-ph"
    libraryDependencyFlag: ""
    libraryPathFlag: "-l"
    linkerScriptFlag: ""

    toolchainDetails: COSMIC.toolchainDetails(qbs)

    knownArchitectures: ["arm", "hcs12", "hcs8", "m68k", "stm8"]

    Rule {
        id: assembler
        inputs: ["asm"]
        outputFileTags: Cpp.assemblerOutputTags(generateAssemblerListingFiles)
        outputArtifacts: Cpp.assemblerOutputArtifacts(input)
        prepare: COSMIC.prepareAssembler.apply(COSMIC, arguments)
    }

    FileTagger {
        patterns: ["*.s"]
        fileTags: ["asm"]
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c"]
        auxiliaryInputs: ["hpp"]
        outputFileTags: Cpp.compilerOutputTags(generateCompilerListingFiles)
        outputArtifacts: Cpp.compilerOutputArtifacts(input)
        prepare: COSMIC.prepareCompiler.apply(COSMIC, arguments)
    }

    Rule {
        id: applicationLinker
        multiplex: true
        inputs: ["obj", "linkerscript"]
        inputsFromDependencies: ["staticlibrary"]
        outputFileTags: Cpp.applicationLinkerOutputTags(generateLinkerMapFile)
        outputArtifacts: Cpp.applicationLinkerOutputArtifacts(product)
        prepare: COSMIC.prepareLinker.apply(COSMIC, arguments)
    }

    Rule {
        id: staticLibraryLinker
        multiplex: true
        inputs: ["obj"]
        inputsFromDependencies: ["staticlibrary"]
        outputFileTags: Cpp.staticLibraryLinkerOutputTags()
        outputArtifacts: Cpp.staticLibraryLinkerOutputArtifacts(product)
        prepare: COSMIC.prepareArchiver.apply(COSMIC, arguments)
    }
}
