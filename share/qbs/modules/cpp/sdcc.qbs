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
import "sdcc.js" as SDCC

CppModule {
    condition: qbs.toolchain && qbs.toolchain.contains("sdcc")

    Probes.BinaryProbe {
        id: compilerPathProbe
        condition: !toolchainInstallPath && !_skipAllChecks
        names: ["sdcc"]
    }

    Probes.SdccProbe {
        id: sdccProbe
        condition: !_skipAllChecks
        compilerFilePath: compilerPath
    }

    qbs.architecture: sdccProbe.found ? sdccProbe.architecture : original

    compilerVersionMajor: sdccProbe.versionMajor
    compilerVersionMinor: sdccProbe.versionMinor
    compilerVersionPatch: sdccProbe.versionPatch
    endianness: sdccProbe.endianness

    compilerDefinesByLanguage: []

    property string toolchainInstallPath: compilerPathProbe.found
        ? compilerPathProbe.path : undefined

    property string compilerExtension: qbs.hostOS.contains("windows") ? ".exe" : ""

    property bool generateMapFile: true
    PropertyOptions {
        name: "generateMapFile"
        description: "produce a linker list file (enabled by default)"
    }

    /* Work-around for QtCreator which expects these properties to exist. */
    property string cCompilerName: compilerName
    property string cxxCompilerName: compilerName

    property string linkerMode: "automatic"

    compilerName: "sdcc" + compilerExtension
    compilerPath: FileInfo.joinPaths(toolchainInstallPath, compilerName)

    assemblerName: {
        switch (qbs.architecture) {
        case "mcs51":
            return "sdas8051" + compilerExtension;
        }
    }
    assemblerPath: FileInfo.joinPaths(toolchainInstallPath, assemblerName)

    linkerName: {
        switch (qbs.architecture) {
        case "mcs51":
            return "sdld" + compilerExtension;
        }
    }
    linkerPath: FileInfo.joinPaths(toolchainInstallPath, linkerName)

    property string archiverName: "sdcclib" + compilerExtension
    property string archiverPath: FileInfo.joinPaths(toolchainInstallPath, archiverName)

    runtimeLibrary: "static"

    staticLibrarySuffix: ".lib"
    executableSuffix: ".ihx"

    property string objectSuffix: ".rel"

    imageFormat: "ihx"

    enableExceptions: false
    enableRtti: false

    Rule {
        id: assembler
        inputs: ["asm"]

        Artifact {
            fileTags: ["obj"]
            filePath: Utilities.getHash(input.baseDir) + "/"
                      + input.fileName + input.cpp.objectSuffix
        }

        prepare: SDCC.prepareAssembler.apply(SDCC, arguments);
    }

    FileTagger {
        condition: qbs.architecture === "arm";
        patterns: "*.s"
        fileTags: ["asm"]
    }

    FileTagger {
        condition: qbs.architecture === "mcs51";
        patterns: ["*.s51", "*.asm"]
        fileTags: ["asm"]
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c"]
        auxiliaryInputs: ["hpp"]

        Artifact {
            fileTags: ["obj"]
            filePath: Utilities.getHash(input.baseDir) + "/"
                      + input.fileName + input.cpp.objectSuffix
        }

        prepare: SDCC.prepareCompiler.apply(SDCC, arguments);
    }

    Rule {
        id: applicationLinker
        multiplex: true
        inputs: ["obj", "linkerscript"]
        inputsFromDependencies: ["staticlibrary"]

        outputFileTags: {
            var tags = ["application"];
            if (product.moduleProperty("cpp", "generateMapFile"))
                tags.push("map_file");
            return tags;
        }
        outputArtifacts: {
            var app = {
                fileTags: ["application"],
                filePath: FileInfo.joinPaths(
                              product.destinationDirectory,
                              PathTools.applicationFilePath(product))
            };
            var artifacts = [app];
            if (product.cpp.generateMapFile) {
                artifacts.push({
                    fileTags: ["map_file"],
                filePath: FileInfo.joinPaths(
                              product.destinationDirectory,
                              product.targetName + ".map")
                });
            }
            return artifacts;
        }

        prepare:SDCC.prepareLinker.apply(SDCC, arguments);
    }

    Rule {
        id: staticLibraryLinker
        multiplex: true
        inputs: ["obj"]
        inputsFromDependencies: ["staticlibrary"]

        Artifact {
            fileTags: ["staticlibrary"]
            filePath: FileInfo.joinPaths(
                            product.destinationDirectory,
                            PathTools.staticLibraryFilePath(product))
        }

        prepare: SDCC.prepareArchiver.apply(SDCC, arguments);
    }
}
