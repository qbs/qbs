/****************************************************************************
**
** Copyright (C) 2022 Denis Shienkov <denis.shienkov@gmail.com>
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

import qbs 1.0
import qbs.File
import qbs.FileInfo
import qbs.ModUtils
import qbs.Probes
import "cpp.js" as Cpp
import "watcom.js" as WATCOM

CppModule {
    condition: qbs.toolchain && qbs.toolchain.includes("watcom")

    Probes.BinaryProbe {
        id: compilerPathProbe
        condition: !toolchainInstallPath && !_skipAllChecks
        names: ["owcc"]
    }

    Probes.WatcomProbe {
        id: watcomProbe
        condition: !_skipAllChecks
        compilerFilePath: compilerPath
        enableDefinesByLanguage: enableCompilerDefinesByLanguage
        _pathListSeparator: qbs.pathListSeparator
        _toolchainInstallPath: toolchainInstallPath
        _targetPlatform: qbs.targetPlatform
        _targetArchitecture: qbs.architecture
    }

    qbs.architecture: watcomProbe.found ? watcomProbe.architecture : original
    qbs.targetPlatform: watcomProbe.found ? watcomProbe.targetPlatform : original

    compilerVersionMajor: watcomProbe.versionMajor
    compilerVersionMinor: watcomProbe.versionMinor
    compilerVersionPatch: watcomProbe.versionPatch
    endianness: watcomProbe.endianness

    compilerDefinesByLanguage: watcomProbe.compilerDefinesByLanguage
    compilerIncludePaths: watcomProbe.includePaths

    toolchainInstallPath: compilerPathProbe.found ? compilerPathProbe.path : undefined

    /* Work-around for QtCreator which expects these properties to exist. */
    property string cCompilerName: compilerName
    property string cxxCompilerName: compilerName

    compilerName: "owcc" + compilerExtension
    compilerPath: FileInfo.joinPaths(toolchainInstallPath, compilerName)

    assemblerName: "wasm" + compilerExtension
    assemblerPath: FileInfo.joinPaths(toolchainInstallPath, assemblerName)

    linkerName: "wlink" + compilerExtension
    linkerPath: FileInfo.joinPaths(toolchainInstallPath, linkerName)

    property string disassemblerName: "wdis" + compilerExtension
    property string disassemblerPath: FileInfo.joinPaths(toolchainInstallPath,
                                                         disassemblerName)
    property string resourceCompilerName: "wrc" + compilerExtension
    property string resourceCompilerPath: FileInfo.joinPaths(toolchainInstallPath,
                                                             resourceCompilerName)
    property string libraryManagerName: "wlib" + compilerExtension
    property string libraryManagerPath: FileInfo.joinPaths(toolchainInstallPath,
                                                           libraryManagerName)

    runtimeLibrary: "dynamic"

    staticLibrarySuffix: ".lib"
    dynamicLibrarySuffix: toolchainDetails.dynamicLibrarySuffix
    executableSuffix: toolchainDetails.executableSuffix
    objectSuffix: ".obj"

    imageFormat: toolchainDetails.imageFormat

    defineFlag: "-D"
    includeFlag: "-I"
    systemIncludeFlag: "-I"
    preincludeFlag: "-include"
    libraryDependencyFlag: "-l"
    libraryPathFlag: "-L"
    linkerScriptFlag: ""

    toolchainDetails: WATCOM.toolchainDetails(qbs)

    knownArchitectures: ["x86", "x86_16"]

    property var buildEnv: watcomProbe.environment
    setupBuildEnvironment: {
        for (var key in product.cpp.buildEnv) {
            var v = new ModUtils.EnvironmentVariable(key, product.qbs.pathListSeparator);
            v.prepend(product.cpp.buildEnv[key]);
            v.set();
        }
    }

    Rule {
        id: assembler
        inputs: ["asm"]
        outputFileTags: Cpp.assemblerOutputTags(generateAssemblerListingFiles)
        outputArtifacts: Cpp.assemblerOutputArtifacts(input)
        prepare: WATCOM.prepareAssembler.apply(WATCOM, arguments)
    }

    FileTagger {
        patterns: ["*.asm"]
        fileTags: ["asm"]
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c"]
        auxiliaryInputs: ["hpp"]
        outputFileTags: Cpp.compilerOutputTags(generateCompilerListingFiles)
        outputArtifacts: Cpp.compilerOutputArtifacts(input)
        prepare: WATCOM.prepareCompiler.apply(WATCOM, arguments)
    }

    Rule {
        id: rccCompiler
        inputs: ["rc"]
        auxiliaryInputs: ["hpp"]
        outputFileTags: Cpp.resourceCompilerOutputTags()
        outputArtifacts: Cpp.resourceCompilerOutputArtifacts(input)
        prepare: WATCOM.prepareResourceCompiler.apply(WATCOM, arguments)
    }

    FileTagger {
        patterns: ["*.rc"]
        fileTags: ["rc"]
    }

    Rule {
        id: applicationLinker
        multiplex: true
        inputs: ["obj", "res", "linkerscript"]
        inputsFromDependencies: ["staticlibrary", "dynamiclibrary_import"]
        outputFileTags: Cpp.applicationLinkerOutputTags(generateLinkerMapFile)
        outputArtifacts: Cpp.applicationLinkerOutputArtifacts(product)
        prepare: WATCOM.prepareLinker.apply(WATCOM, arguments)
    }

    Rule {
        id: dynamicLibraryLinker
        condition: qbs.targetOS.includes("windows")
        multiplex: true
        inputs: ["obj", "res"]
        inputsFromDependencies: ["staticlibrary", "dynamiclibrary_import"]
        outputFileTags: Cpp.dynamicLibraryLinkerOutputTags();
        outputArtifacts: Cpp.dynamicLibraryLinkerOutputArtifacts(product)
        prepare: WATCOM.prepareLinker.apply(WATCOM, arguments)
    }

    Rule {
        id: libraryManager
        multiplex: true
        inputs: ["obj"]
        inputsFromDependencies: ["staticlibrary", "dynamiclibrary_import"]
        outputFileTags: Cpp.staticLibraryLinkerOutputTags()
        outputArtifacts: Cpp.staticLibraryLinkerOutputArtifacts(product)
        prepare: WATCOM.prepareLibraryManager.apply(WATCOM, arguments)
    }

    JobLimit {
        jobPool: "watcom_job_pool"
        jobCount: 1
    }
}
