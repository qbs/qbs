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

import qbs 1.0
import qbs.File
import qbs.FileInfo
import qbs.ModUtils
import qbs.Probes
import "keil.js" as KEIL

CppModule {
    condition: qbs.hostOS.contains("windows") && qbs.toolchain && qbs.toolchain.contains("keil")

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

    compilerVersionMajor: keilProbe.versionMajor
    compilerVersionMinor: keilProbe.versionMinor
    compilerVersionPatch: keilProbe.versionPatch
    endianness: keilProbe.endianness

    compilerDefinesByLanguage: keilProbe.compilerDefinesByLanguage
    compilerIncludePaths: keilProbe.includePaths

    property string toolchainInstallPath: compilerPathProbe.found
        ? compilerPathProbe.path : undefined

    property string compilerExtension: qbs.hostOS.contains("windows") ? ".exe" : ""

    /* Work-around for QtCreator which expects these properties to exist. */
    property string cCompilerName: compilerName
    property string cxxCompilerName: compilerName

    compilerName: KEIL.compilerName(qbs) + compilerExtension
    compilerPath: FileInfo.joinPaths(toolchainInstallPath, compilerName)

    assemblerName: KEIL.assemblerName(qbs) + compilerExtension
    assemblerPath: FileInfo.joinPaths(toolchainInstallPath, assemblerName)

    linkerName: KEIL.linkerName(qbs) + compilerExtension
    linkerPath: FileInfo.joinPaths(toolchainInstallPath, linkerName)

    property string archiverName: KEIL.archiverName(qbs) + compilerExtension
    property string archiverPath: FileInfo.joinPaths(toolchainInstallPath, archiverName)

    runtimeLibrary: "static"

    staticLibrarySuffix: KEIL.staticLibrarySuffix(qbs)
    executableSuffix: KEIL.executableSuffix(qbs)

    property string objectSuffix: KEIL.objectSuffix(qbs)
    property string mapFileSuffix: KEIL.mapFileSuffix(qbs)

    imageFormat: KEIL.imageFormat(qbs)

    enableExceptions: false
    enableRtti: false

    Rule {
        id: assembler
        inputs: ["asm"]
        outputFileTags: ["obj", "lst"]
        outputArtifacts: KEIL.compilerOutputArtifacts(
            input, input.cpp.generateAssemblerListingFiles)
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
        outputFileTags: ["obj", "lst"]
        outputArtifacts: KEIL.compilerOutputArtifacts(
            input, input.cpp.generateCompilerListingFiles)
        prepare: KEIL.prepareCompiler.apply(KEIL, arguments)
    }

    Rule {
        id: applicationLinker
        multiplex: true
        inputs: ["obj", "linkerscript"]
        outputFileTags: ["application", "mem_map"]
        outputArtifacts: KEIL.applicationLinkerOutputArtifacts(product)
        prepare: KEIL.prepareLinker.apply(KEIL, arguments)
    }

    Rule {
        id: staticLibraryLinker
        multiplex: true
        inputs: ["obj"]
        inputsFromDependencies: ["staticlibrary"]
        outputFileTags: ["staticlibrary"]
        outputArtifacts: KEIL.staticLibraryLinkerOutputArtifacts(product)
        prepare: KEIL.prepareArchiver.apply(KEIL, arguments)
    }
}
