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
        name: "cpp_compiler"
        inputs: ["cpp", "cppm", "c"]
        auxiliaryInputs: ["hpp"]
        explicitlyDependsOn: ["c_pch", "cpp_pch"]
        outputFileTags: Cpp.compilerOutputTags(generateCompilerListingFiles, /*withCxxModules*/ true)
        outputArtifacts: Cpp.compilerOutputArtifacts(input, undefined, /*withCxxModules*/ true)
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
        outputArtifacts: MSVC.appLinkerOutputArtifacts(product)
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
        outputArtifacts: MSVC.dllLinkerOutputArtifacts(product)
        prepare: MSVC.prepareLinker.apply(MSVC, arguments)
    }

    Rule {
        name: "libtool"
        multiplex: true
        inputs: ["obj", "res"]
        inputsFromDependencies: ["staticlibrary", "dynamiclibrary_import"]
        outputFileTags: ["staticlibrary", "debuginfo_cl"]
        outputArtifacts: MSVC.libtoolOutputArtifacts(product)
        prepare: MSVC.libtoolCommands.apply(MSVC, arguments)
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
        prepare: MSVC.assemblerCommands.apply(MSVC, arguments)
    }
}
