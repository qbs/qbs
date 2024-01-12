/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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
import qbs.ModUtils
import qbs.PathTools
import qbs.Probes
import qbs.Utilities
import qbs.WindowsUtils
import 'cpp.js' as Cpp
import 'msvc.js' as MSVC

CppModule {
    condition: false

    Depends { name: "codesign" }

    windowsApiCharacterSet: "unicode"
    platformDefines: {
        var defines = base.concat(WindowsUtils.characterSetDefines(windowsApiCharacterSet))
                          .concat("WIN32");
        var def = WindowsUtils.winapiFamilyDefine(windowsApiFamily);
        if (def)
            defines.push("WINAPI_FAMILY=WINAPI_FAMILY_" + def);
        (windowsApiAdditionalPartitions || []).map(function (name) {
            defines.push("WINAPI_PARTITION_" + WindowsUtils.winapiPartitionDefine(name) + "=1");
        });
        return defines;
    }
    platformCommonCompilerFlags: {
        var flags = base;
        if (compilerVersionMajor >= 18) // 2013
            flags.push("/FS");
        return flags;
    }
    warningLevel: "default"
    compilerName: "cl.exe"
    compilerPath: FileInfo.joinPaths(toolchainInstallPath, compilerName)
    assemblerName: {
        switch (qbs.architecture) {
        case "armv7":
            return "armasm.exe";
        case "arm64":
            return "armasm64.exe";
        case "ia64":
            return "ias.exe";
        case "x86":
            return "ml.exe";
        case "x86_64":
            return "ml64.exe";
        }
    }

    linkerName: "link.exe"
    runtimeLibrary: "dynamic"
    separateDebugInformation: true

    property bool generateManifestFile: true

    architecture: qbs.architecture
    endianness: "little"
    staticLibrarySuffix: ".lib"
    dynamicLibrarySuffix: ".dll"
    executableSuffix: ".exe"
    debugInfoSuffix: ".pdb"
    objectSuffix: ".obj"
    precompiledHeaderSuffix: ".pch"
    imageFormat: "pe"
    Properties {
        condition: product.multiplexByQbsProperties.includes("buildVariants")
                   && qbs.buildVariants && qbs.buildVariants.length > 1
                   && qbs.buildVariant !== "release"
                   && product.type.containsAny(["staticlibrary", "dynamiclibrary"])
        variantSuffix: "d"
    }

    property var buildEnv

    readonly property bool shouldSignArtifacts: codesign.enableCodeSigning
    property bool enableCxxLanguageMacro: false

    setupBuildEnvironment: {
        for (var key in product.cpp.buildEnv) {
            var v = new ModUtils.EnvironmentVariable(key, ';');
            v.prepend(product.cpp.buildEnv[key]);
            v.set();
        }
    }

    property string windowsSdkVersion

    defineFlag: "/D"
    includeFlag: "/I"
    systemIncludeFlag: "/external:I"
    preincludeFlag: "/FI"
    libraryPathFlag: "/LIBPATH:"

    Rule {
        condition: useCPrecompiledHeader
        inputs: ["c_pch_src"]
        auxiliaryInputs: ["hpp"]
        outputFileTags: Cpp.precompiledHeaderOutputTags("c", true)
        outputArtifacts: Cpp.precompiledHeaderOutputArtifacts(input, product, "c", true)
        prepare: MSVC.prepareCompiler.apply(MSVC, arguments)
    }

    Rule {
        condition: useCxxPrecompiledHeader
        inputs: ["cpp_pch_src"]
        explicitlyDependsOn: ["c_pch"]  // to prevent vc--0.pdb conflict
        auxiliaryInputs: ["hpp"]
        outputFileTags: Cpp.precompiledHeaderOutputTags("cpp", true)
        outputArtifacts: Cpp.precompiledHeaderOutputArtifacts(input, product, "cpp", true)
        prepare: MSVC.prepareCompiler.apply(MSVC, arguments)
    }

    Rule {
        name: "compiler"
        inputs: ["cpp", "c"]
        auxiliaryInputs: ["hpp"]
        explicitlyDependsOn: ["c_pch", "cpp_pch"]
        outputFileTags: Cpp.compilerOutputTags(generateCompilerListingFiles)
        outputArtifacts: Cpp.compilerOutputArtifacts(input)
        prepare: MSVC.prepareCompiler.apply(MSVC, arguments)
    }

    FileTagger {
        patterns: ["*.manifest"]
        fileTags: ["native.pe.manifest"]
    }

    FileTagger {
        patterns: ["*.def"]
        fileTags: ["def"]
    }

    Rule {
        name: "applicationLinker"
        multiplex: true
        inputs: ['obj', 'res', 'native.pe.manifest', 'def']
        inputsFromDependencies: ['staticlibrary', 'dynamiclibrary_import', "debuginfo_app"]

        outputFileTags: {
            var tags = ["application", "debuginfo_app"];
            if (generateLinkerMapFile)
                tags.push("mem_map");
            if (shouldSignArtifacts)
                tags.push("codesign.signed_artifact");
            return tags;
        }
        outputArtifacts: {
            var app = {
                fileTags: ["application"].concat(
                    product.cpp.shouldSignArtifacts ? ["codesign.signed_artifact"] : []),
                filePath: FileInfo.joinPaths(
                              product.destinationDirectory,
                              PathTools.applicationFilePath(product))
            };
            var artifacts = [app];
            if (product.cpp.debugInformation && product.cpp.separateDebugInformation) {
                artifacts.push({
                    fileTags: ["debuginfo_app"],
                    filePath: app.filePath.substr(0, app.filePath.length - 4)
                              + product.cpp.debugInfoSuffix
                });
            }
            if (product.cpp.generateLinkerMapFile) {
                artifacts.push({
                    fileTags: ["mem_map"],
                    filePath: FileInfo.joinPaths(
                                  product.destinationDirectory,
                                  product.targetName + product.cpp.linkerMapSuffix)
                });
            }
            return artifacts;
        }

        prepare: MSVC.prepareLinker.apply(MSVC, arguments)
    }

    Rule {
        name: "dynamicLibraryLinker"
        multiplex: true
        inputs: ['obj', 'res', 'native.pe.manifest', 'def']
        inputsFromDependencies: ['staticlibrary', 'dynamiclibrary_import', "debuginfo_dll"]

        outputFileTags: {
            var tags = ["dynamiclibrary", "dynamiclibrary_import", "debuginfo_dll"];
            if (shouldSignArtifacts)
                tags.push("codesign.signed_artifact");
            return tags;
        }
        outputArtifacts: {
            var artifacts = [
                {
                    fileTags: ["dynamiclibrary"].concat(
                        product.cpp.shouldSignArtifacts ? ["codesign.signed_artifact"] : []),
                    filePath: FileInfo.joinPaths(product.destinationDirectory,
                                                 PathTools.dynamicLibraryFilePath(product))
                },
                {
                    fileTags: ["dynamiclibrary_import"],
                    filePath: FileInfo.joinPaths(product.destinationDirectory,
                                                 PathTools.importLibraryFilePath(product)),
                    alwaysUpdated: false
                }
            ];
            if (product.cpp.debugInformation && product.cpp.separateDebugInformation) {
                var lib = artifacts[0];
                artifacts.push({
                    fileTags: ["debuginfo_dll"],
                    filePath: lib.filePath.substr(0, lib.filePath.length - 4)
                              + product.cpp.debugInfoSuffix
                });
            }
            return artifacts;
        }

        prepare: MSVC.prepareLinker.apply(MSVC, arguments)
    }

    Rule {
        name: "libtool"
        multiplex: true
        inputs: ["obj", "res"]
        inputsFromDependencies: ["staticlibrary", "dynamiclibrary_import"]
        outputFileTags: ["staticlibrary", "debuginfo_cl"]
        outputArtifacts: {
            var artifacts = [
                {
                    fileTags: ["staticlibrary"],
                    filePath: FileInfo.joinPaths(product.destinationDirectory,
                                                 PathTools.staticLibraryFilePath(product))
                }
            ];
            if (product.cpp.debugInformation && product.cpp.separateDebugInformation) {
                artifacts.push({
                    fileTags: ["debuginfo_cl"],
                    filePath: product.targetName + ".cl" + product.cpp.debugInfoSuffix
                });
            }
            return artifacts;
        }
        prepare: {
            var args = ['/nologo']
            var lib = outputs["staticlibrary"][0];
            var nativeOutputFileName = FileInfo.toWindowsSeparators(lib.filePath)
            args.push('/OUT:' + nativeOutputFileName)
            for (var i in inputs.obj) {
                var fileName = FileInfo.toWindowsSeparators(inputs.obj[i].filePath)
                args.push(fileName)
            }
            for (var i in inputs.res) {
                var fileName = FileInfo.toWindowsSeparators(inputs.res[i].filePath)
                args.push(fileName)
            }
            var cmd = new Command("lib.exe", args);
            cmd.description = 'creating ' + lib.fileName;
            cmd.highlight = 'linker';
            cmd.jobPool = "linker";
            cmd.workingDirectory = FileInfo.path(lib.filePath)
            cmd.responseFileUsagePrefix = '@';
            return cmd;
         }
    }

    FileTagger {
        patterns: ["*.rc"]
        fileTags: ["rc"]
    }

    Rule {
        inputs: ["rc"]
        auxiliaryInputs: ["hpp"]
        outputFileTags: Cpp.resourceCompilerOutputTags()
        outputArtifacts: Cpp.resourceCompilerOutputArtifacts(input)
        prepare: {
            // From MSVC 2010 on, the logo can be suppressed.
            var logo = product.cpp.compilerVersionMajor >= 16
                    ? "can-suppress-logo" : "always-shows-logo";
            return MSVC.createRcCommand("rc", input, output, logo);
        }
    }

    FileTagger {
        patterns: "*.asm"
        fileTags: ["asm"]
    }

    Rule {
        inputs: ["asm"]
        outputFileTags: Cpp.assemblerOutputTags(false)
        outputArtifacts: Cpp.assemblerOutputArtifacts(input)
        prepare: {
            var args = ["/nologo", "/c", "/Fo" + FileInfo.toWindowsSeparators(output.filePath)];
            if (product.cpp.debugInformation)
                args.push("/Zi");
            args = args.concat(Cpp.collectMiscAssemblerArguments(input, "asm"));
            args.push(FileInfo.toWindowsSeparators(input.filePath));
            var cmd = new Command(product.cpp.assemblerPath, args);
            cmd.description = "assembling " + input.fileName;
            cmd.jobPool = "assembler";
            cmd.inputFileName = input.fileName;
            cmd.stdoutFilterFunction = function(output) {
                var lines = output.split("\r\n").filter(function (s) {
                    return !s.endsWith(inputFileName); });
                return lines.join("\r\n");
            };
            return cmd;
        }
    }
}
