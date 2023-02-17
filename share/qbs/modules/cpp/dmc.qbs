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
import qbs.Host
import qbs.PathTools
import qbs.Probes
import qbs.Utilities
import "dmc.js" as DMC
import "cpp.js" as Cpp

CppModule {
    condition: Host.os().includes("windows") && qbs.toolchain && qbs.toolchain.includes("dmc")

    Probes.BinaryProbe {
        id: compilerPathProbe
        condition: !toolchainInstallPath && !_skipAllChecks
        names: ["dmc"]
    }

    Probes.DmcProbe {
        id: dmcProbe
        condition: !_skipAllChecks
        compilerFilePath: compilerPath
        enableDefinesByLanguage: enableCompilerDefinesByLanguage
        _targetPlatform: qbs.targetPlatform
        _targetArchitecture: qbs.architecture
        _targetExtender: extenderName
    }

    qbs.architecture: dmcProbe.found ? dmcProbe.architecture : original
    qbs.targetPlatform: dmcProbe.found ? dmcProbe.targetPlatform : original

    compilerVersionMajor: dmcProbe.versionMajor
    compilerVersionMinor: dmcProbe.versionMinor
    compilerVersionPatch: dmcProbe.versionPatch
    endianness: "little"

    compilerDefinesByLanguage: dmcProbe.compilerDefinesByLanguage
    compilerIncludePaths: dmcProbe.includePaths

    toolchainInstallPath: compilerPathProbe.found ? compilerPathProbe.path : undefined

    /* Work-around for QtCreator which expects these properties to exist. */
    property string cCompilerName: compilerName
    property string cxxCompilerName: compilerName

    compilerName: "dmc.exe"
    compilerPath: FileInfo.joinPaths(toolchainInstallPath, compilerName)

    assemblerName: "dmc.exe"
    assemblerPath: FileInfo.joinPaths(toolchainInstallPath, assemblerName)

    linkerName: "link.exe"
    linkerPath: FileInfo.joinPaths(toolchainInstallPath, linkerName)

    property string archiverName: "lib.exe"
    property string archiverPath: FileInfo.joinPaths(toolchainInstallPath, archiverName)
    property string rccCompilerName: "rcc.exe"
    property string rccCompilerPath: FileInfo.joinPaths(toolchainInstallPath, rccCompilerName)

    property string extenderName
    PropertyOptions {
        name: "extenderName"
        allowedValues: [undefined, "dosz", "dosr", "dosx", "dosp"]
        description: "Specifies the DOS memory extender to compile with:\n"
            + " - \"dosz\" is the ZPM 16 bit DOS Extender\n"
            + " - \"dosr\" is the Rational 16 bit DOS Extender\n"
            + " - \"dosx\" is the DOSX 32 bit DOS Extender\n"
            + " - \"dosp\" is the Pharlap 32 bit DOS Extender\n"
        ;
    }

    runtimeLibrary: "dynamic"

    staticLibrarySuffix: ".lib"
    dynamicLibrarySuffix: ".dll"
    executableSuffix: ".exe"
    objectSuffix: ".obj"

    imageFormat: {
        if (qbs.targetPlatform === "dos")
            return "mz";
        else if (qbs.targetPlatform === "windows")
            return "pe";
    }

    defineFlag: "-D"
    includeFlag: "-I"
    systemIncludeFlag: "-I"
    preincludeFlag: "-HI"
    libraryPathFlag: "-L/packcode"

    knownArchitectures: ["x86", "x86_16"]

    Rule {
        id: assembler
        inputs: ["asm"]
        outputFileTags: DMC.depsOutputTags().concat(
                            Cpp.assemblerOutputTags(generateAssemblerListingFiles))
        outputArtifacts: DMC.depsOutputArtifacts(input, product).concat(
                             Cpp.assemblerOutputArtifacts(input))
        prepare: DMC.prepareAssembler.apply(DMC, arguments)
    }

    FileTagger {
        patterns: ["*.s", "*.asm"]
        fileTags: ["asm"]
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c"]
        auxiliaryInputs: ["hpp"]
        outputFileTags: DMC.depsOutputTags().concat(
                            Cpp.compilerOutputTags(generateCompilerListingFiles))
        outputArtifacts: DMC.depsOutputArtifacts(input, product).concat(
                             Cpp.compilerOutputArtifacts(input))
        prepare: DMC.prepareCompiler.apply(DMC, arguments)
    }


    Rule {
        id: rccCompiler
        inputs: ["rc"]
        auxiliaryInputs: ["hpp"]
        outputFileTags: Cpp.resourceCompilerOutputTags()
        outputArtifacts: Cpp.resourceCompilerOutputArtifacts(input)
        prepare: DMC.prepareRccCompiler.apply(DMC, arguments)
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
        prepare: DMC.prepareLinker.apply(DMC, arguments)
    }

    Rule {
        id: dynamicLibraryLinker
        multiplex: true
        inputs: ["obj", "res"]
        inputsFromDependencies: ["staticlibrary", "dynamiclibrary_import"]
        outputFileTags: Cpp.dynamicLibraryLinkerOutputTags()
        outputArtifacts: Cpp.dynamicLibraryLinkerOutputArtifacts(product)
        prepare: DMC.prepareLinker.apply(DMC, arguments)
    }

    Rule {
        id: staticLibraryLinker
        multiplex: true
        inputs: ["obj"]
        inputsFromDependencies: ["staticlibrary", "dynamiclibrary_import"]
        outputFileTags: Cpp.staticLibraryLinkerOutputTags()
        outputArtifacts: Cpp.staticLibraryLinkerOutputArtifacts(product)
        prepare: DMC.prepareArchiver.apply(DMC, arguments)
    }
}
