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
import qbs.PathTools
import qbs.Probes
import qbs.Utilities
import "iar.js" as IAR

CppModule {
    condition: qbs.toolchain && qbs.toolchain.contains("iar")

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

    compilerVersionMajor: iarProbe.versionMajor
    compilerVersionMinor: iarProbe.versionMinor
    compilerVersionPatch: iarProbe.versionPatch
    endianness: iarProbe.endianness

    compilerDefinesByLanguage: iarProbe.compilerDefinesByLanguage
    compilerIncludePaths: iarProbe.includePaths

    property string toolchainInstallPath: compilerPathProbe.found
        ? compilerPathProbe.path : undefined

    property string compilerExtension: qbs.hostOS.contains("windows") ? ".exe" : ""

    /* Work-around for QtCreator which expects these properties to exist. */
    property string cCompilerName: compilerName
    property string cxxCompilerName: compilerName

    compilerName: IAR.compilerName(qbs) + compilerExtension
    compilerPath: FileInfo.joinPaths(toolchainInstallPath, compilerName)

    assemblerName: IAR.assemblerName(qbs) + compilerExtension
    assemblerPath: FileInfo.joinPaths(toolchainInstallPath, assemblerName)

    linkerName: IAR.linkerName(qbs) + compilerExtension
    linkerPath: FileInfo.joinPaths(toolchainInstallPath, linkerName)

    property string archiverName: IAR.archiverName(qbs) + compilerExtension
    property string archiverPath: FileInfo.joinPaths(toolchainInstallPath, archiverName)

    runtimeLibrary: "static"

    staticLibrarySuffix: IAR.staticLibrarySuffix(qbs)
    executableSuffix: IAR.executableSuffix(qbs)

    property string objectSuffix: IAR.objectSuffix(qbs)

    imageFormat: IAR.imageFormat(qbs)

    enableExceptions: false
    enableRtti: false

    Rule {
        id: assembler
        inputs: ["asm"]
        outputFileTags: ["obj", "lst"]
        outputArtifacts: IAR.compilerOutputArtifacts(
            input, input.cpp.generateAssemblerListingFiles)
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
        outputFileTags: ["obj", "lst"]
        outputArtifacts: IAR.compilerOutputArtifacts(
            input, input.cpp.generateCompilerListingFiles)
        prepare: IAR.prepareCompiler.apply(IAR, arguments)
    }

    Rule {
        id: applicationLinker
        multiplex: true
        inputs: ["obj", "linkerscript"]
        inputsFromDependencies: ["staticlibrary"]
        outputFileTags: ["application", "mem_map"]
        outputArtifacts: IAR.applicationLinkerOutputArtifacts(product)
        prepare: IAR.prepareLinker.apply(IAR, arguments)
    }

    Rule {
        id: staticLibraryLinker
        multiplex: true
        inputs: ["obj"]
        inputsFromDependencies: ["staticlibrary"]
        outputFileTags: ["staticlibrary"]
        outputArtifacts: IAR.staticLibraryLinkerOutputArtifacts(product)
        prepare: IAR.prepareArchiver.apply(IAR, arguments)
    }
}
