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

import qbs 1.0
import qbs.File
import qbs.FileInfo
import qbs.ModUtils
import qbs.PathTools
import qbs.Probes
import qbs.Process
import qbs.Utilities
import qbs.UnixUtils
import qbs.WindowsUtils
import 'gcc.js' as Gcc

CppModule {
    condition: false

    Probes.GccProbe {
        id: gccProbe
        compilerFilePath: compilerPath
        preferredArchitecture: targetArch
        preferredMachineType: machineType
    }

    qbs.architecture: gccProbe.found ? gccProbe.architecture : original

    compilerVersionMajor: gccProbe.versionMajor
    compilerVersionMinor: gccProbe.versionMinor
    compilerVersionPatch: gccProbe.versionPatch

    compilerIncludePaths: gccProbe.includePaths
    compilerFrameworkPaths: gccProbe.frameworkPaths
    compilerLibraryPaths: gccProbe.libraryPaths

    property string target: [targetArch, targetVendor, targetSystem, targetAbi].join("-")
    property string targetArch: Utilities.canonicalTargetArchitecture(
                                    qbs.architecture, targetVendor, targetSystem, targetAbi)
    property string targetVendor: "unknown"
    property string targetSystem: "unknown"
    property string targetAbi: "unknown"

    property string toolchainPrefix
    property path toolchainInstallPath
    assemblerName: 'as'
    compilerName: cxxCompilerName
    linkerName: 'ld'
    property string archiverName: 'ar'
    property string nmName: 'nm'
    property string objcopyName: "objcopy"
    property string stripName: "strip"
    property string dsymutilName: "dsymutil"
    property path sysroot: qbs.sysroot

    property string linkerMode: "automatic"
    PropertyOptions {
        name: "linkerMode"
        allowedValues: ["automatic", "manual"]
        description: "Controls whether to automatically use an appropriate compiler frontend "
            + "in place of the system linker when linking binaries. The default is \"automatic\", "
            + "which chooses either the C++ compiler, C compiler, or system linker specified by "
            + "the linkerName/linkerPath properties, depending on the type of object files "
            + "present on the linker command line. \"manual\" allows you to explicitly specify "
            + "the linker using the linkerName/linkerPath properties, and allows linker flags "
            + "passed to the linkerFlags and platformLinkerFlags properties to be escaped "
            + "manually (using -Wl or -Xlinker) instead of automatically based on the selected "
            + "linker."
    }

    property string exportedSymbolsCheckMode: "ignore-undefined"
    PropertyOptions {
        name: "exportedSymbolsCheckMode"
        allowedValues: ["strict", "ignore-undefined"]
        description: "Controls when we consider an updated dynamic library as changed with "
            + "regards to other binaries depending on it. The default is \"ignore-undefined\", "
            + "which means we do not care about undefined symbols being added or removed. "
            + "If you do care about that, e.g. because you link dependent products with an option "
            + "such as \"--no-undefined\", then you should set this property to \"strict\"."
    }

    property string toolchainPathPrefix: {
        var path = ''
        if (toolchainInstallPath) {
            path += toolchainInstallPath
            if (path.substr(-1) !== '/')
                path += '/'
        }
        if (toolchainPrefix)
            path += toolchainPrefix
        return path
    }

    property string compilerExtension: qbs.hostOS.contains("windows") ? ".exe" : ""
    property string cCompilerName: (qbs.toolchain.contains("clang") ? "clang" : "gcc")
                                   + compilerExtension
    property string cxxCompilerName: (qbs.toolchain.contains("clang") ? "clang++" : "g++")
                                     + compilerExtension

    compilerPathByLanguage: ({
        "c": toolchainPathPrefix + cCompilerName,
        "cpp": toolchainPathPrefix + cxxCompilerName,
        "objc": toolchainPathPrefix + cCompilerName,
        "objcpp": toolchainPathPrefix + cxxCompilerName,
        "asm_cpp": toolchainPathPrefix + cCompilerName
    })

    assemblerPath: toolchainPathPrefix + assemblerName
    compilerPath: toolchainPathPrefix + compilerName
    linkerPath: toolchainPathPrefix + linkerName
    property string archiverPath: toolchainPathPrefix + archiverName
    property string nmPath: toolchainPathPrefix + nmName
    property string objcopyPath: toolchainPathPrefix + objcopyName
    property string stripPath: toolchainPathPrefix + stripName
    property string dsymutilPath: toolchainPathPrefix + dsymutilName
    property stringList dsymutilFlags

    readonly property bool shouldCreateSymlinks: {
        return createSymlinks && internalVersion && ["macho", "elf"].contains(cpp.imageFormat);
    }

    readonly property string internalVersion: {
        if (product.version === undefined)
            return undefined;

        if (!Gcc.isNumericProductVersion(product.version)) {
            // Dynamic library version numbers like "A" or "B" are common on Apple platforms, so
            // don't restrict the product version to a componentized version number here.
            if (cpp.imageFormat === "macho")
                return product.version;

            throw("product.version must be a string in the format x[.y[.z[.w]] "
                  + "where each component is an integer");
        }

        var maxVersionParts = 3;
        var versionParts = product.version.split('.').slice(0, maxVersionParts);

        // pad if necessary
        for (var i = versionParts.length; i < maxVersionParts; ++i)
            versionParts.push("0");

        return versionParts.join('.');
    }
    property string soVersion: {
        var v = internalVersion;
        if (!Gcc.isNumericProductVersion(v))
            return "";
        return v.split('.')[0];
    }

    exceptionHandlingModel: {
        if (qbs.toolchain.contains("mingw")) {
            // https://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html claims
            // __USING_SJLJ_EXCEPTIONS__ is defined as 1 when using SJLJ exceptions, but there don't
            // seem to be defines for the other models, so use the presence of the DLLs for now.
            var prefix = toolchainInstallPath;
            if (!qbs.hostOS.contains("windows"))
                prefix = FileInfo.joinPaths(toolchainInstallPath, "..", "lib", "gcc",
                                            toolchainPrefix,
                                            [compilerVersionMajor, compilerVersionMinor].join("."));
            var models = ["seh", "sjlj", "dw2"];
            for (var i = 0; i < models.length; ++i) {
                if (File.exists(FileInfo.joinPaths(prefix, "libgcc_s_" + models[i] + "-1.dll"))) {
                    return models[i];
                }
            }
        }
        return base;
    }

    validate: {
        var validator = new ModUtils.PropertyValidator("cpp");
        validator.setRequiredProperty("architecture", architecture,
                                      "you might want to re-run 'qbs-setup-toolchains'");
        if (gccProbe.architecture) {
            validator.addCustomValidator("architecture", architecture, function (value) {
                return Utilities.canonicalArchitecture(architecture) === Utilities.canonicalArchitecture(gccProbe.architecture);
            }, "'" + architecture + "' differs from the architecture produced by this compiler (" +
            gccProbe.architecture +")");
        } else {
            // This is a warning and not an error on the rare chance some new architecture comes
            // about which qbs does not know about the macros of. But it *might* still work.
            if (architecture)
                console.warn("Unknown architecture '" + architecture + "' " +
                             "may not be supported by this compiler.");
        }

        var validateFlagsFunction = function (value) {
            if (value) {
                for (var i = 0; i < value.length; ++i) {
                    if (["-target", "-triple", "-arch"].contains(value[i])
                            || value[i].startsWith("-march="))
                        return false;
                }
            }
            return true;
        }

        var msg = "'-target', '-triple', '-arch' and '-march' cannot appear in flags; set qbs.architecture instead";
        validator.addCustomValidator("assemblerFlags", assemblerFlags, validateFlagsFunction, msg);
        validator.addCustomValidator("cppFlags", cppFlags, validateFlagsFunction, msg);
        validator.addCustomValidator("cFlags", cFlags, validateFlagsFunction, msg);
        validator.addCustomValidator("cxxFlags", cxxFlags, validateFlagsFunction, msg);
        validator.addCustomValidator("objcFlags", objcFlags, validateFlagsFunction, msg);
        validator.addCustomValidator("objcxxFlags", objcxxFlags, validateFlagsFunction, msg);
        validator.addCustomValidator("commonCompilerFlags", commonCompilerFlags, validateFlagsFunction, msg);
        validator.addCustomValidator("platformAssemblerFlags", platformAssemblerFlags, validateFlagsFunction, msg);
        //validator.addCustomValidator("platformCppFlags", platformCppFlags, validateFlagsFunction, msg);
        validator.addCustomValidator("platformCFlags", platformCFlags, validateFlagsFunction, msg);
        validator.addCustomValidator("platformCxxFlags", platformCxxFlags, validateFlagsFunction, msg);
        validator.addCustomValidator("platformObjcFlags", platformObjcFlags, validateFlagsFunction, msg);
        validator.addCustomValidator("platformObjcxxFlags", platformObjcxxFlags, validateFlagsFunction, msg);
        validator.addCustomValidator("platformCommonCompilerFlags", platformCommonCompilerFlags, validateFlagsFunction, msg);

        validator.setRequiredProperty("compilerVersion", compilerVersion);
        validator.setRequiredProperty("compilerVersionMajor", compilerVersionMajor);
        validator.setRequiredProperty("compilerVersionMinor", compilerVersionMinor);
        validator.setRequiredProperty("compilerVersionPatch", compilerVersionPatch);
        validator.addVersionValidator("compilerVersion", compilerVersion, 3, 3);
        validator.addRangeValidator("compilerVersionMajor", compilerVersionMajor, 1);
        validator.addRangeValidator("compilerVersionMinor", compilerVersionMinor, 0);
        validator.addRangeValidator("compilerVersionPatch", compilerVersionPatch, 0);

        validator.setRequiredProperty("compilerIncludePaths", compilerIncludePaths);
        validator.setRequiredProperty("compilerFrameworkPaths", compilerFrameworkPaths);
        validator.setRequiredProperty("compilerLibraryPaths", compilerLibraryPaths);

        validator.validate();
    }

    Rule {
        id: dynamicLibraryLinker
        multiplex: true
        inputs: {
            var tags = ["obj", "linkerscript", "versionscript"];
            if (product.type.contains("dynamiclibrary") &&
                product.moduleProperty("qbs", "targetOS").contains("darwin") &&
                product.moduleProperty("bundle", "embedInfoPlist"))
                tags.push("aggregate_infoplist");
            return tags;
        }
        inputsFromDependencies: ["dynamiclibrary_copy", "staticlibrary"]

        outputFileTags: [
            "dynamiclibrary", "dynamiclibrary_symlink", "dynamiclibrary_copy", "debuginfo_dll"
        ]
        outputArtifacts: {
            var lib = {
                filePath: product.destinationDirectory + "/"
                          + PathTools.dynamicLibraryFilePath(product),
                fileTags: ["dynamiclibrary"]
            };
            var libCopy = {
                // Copy of libfoo for smart re-linking.
                filePath: product.destinationDirectory + "/.socopy/"
                          + PathTools.dynamicLibraryFilePath(product),
                fileTags: ["dynamiclibrary_copy"],
                alwaysUpdated: false,
            };
            var artifacts = [lib, libCopy];

            if (ModUtils.moduleProperty(product, "shouldCreateSymlinks") && !product.moduleProperty("bundle", "isBundle")) {
                var maxVersionParts = Gcc.isNumericProductVersion(product.version) ? 3 : 1;
                for (var i = 0; i < maxVersionParts; ++i) {
                    var symlink = {
                        filePath: product.destinationDirectory + "/"
                                  + PathTools.dynamicLibraryFileName(product, undefined, i),
                        fileTags: ["dynamiclibrary_symlink"]
                    };
                    if (i > 0 && artifacts[i-1].filePath == symlink.filePath)
                        break; // Version number has less than three components.
                    artifacts.push(symlink);
                }
            }
            return artifacts.concat(Gcc.debugInfoArtifacts(product, "dll"));
        }

        prepare: {
            return Gcc.prepareLinker.apply(this, arguments);
        }
    }

    Rule {
        id: staticLibraryLinker
        multiplex: true
        inputs: ["obj", "linkerscript"]
        inputsFromDependencies: ["dynamiclibrary", "staticlibrary"]

        outputFileTags: ["staticlibrary", "c_staticlibrary", "cpp_staticlibrary"]
        outputArtifacts: {
            var tags = ["staticlibrary"];
            for (var i = 0; i < inputs["obj"].length; ++i) {
                var ft = inputs["obj"][i].fileTags;
                if (ft.contains("c_obj"))
                    tags.push("c_staticlibrary");
                if (ft.contains("cpp_obj"))
                    tags.push("cpp_staticlibrary");
            }

            var staticLibraries = function() {
                var result = [];
                for (var i in inputs.staticlibrary) {
                    var lib = inputs.staticlibrary[i]
                    result = Gcc.concatLibs(result, [lib.filePath].concat(
                                            ModUtils.moduleProperty(lib, 'staticLibraries')));
                }
                result = Gcc.concatLibs(result,
                                        ModUtils.moduleProperty(product, 'staticLibraries'));
                return result
            }();

            var dynamicLibraries = function() {
                var result = [];
                for (var i in inputs.dynamiclibrary)
                    result.push(inputs.dynamiclibrary[i].filePath);
                result = result.concat(ModUtils.moduleProperty(product, 'dynamicLibraries'));
                return result
            }();

            return [{
                filePath: FileInfo.joinPaths(product.destinationDirectory,
                                             PathTools.staticLibraryFilePath(product)),
                fileTags: tags,
                cpp: { "staticLibraries": staticLibraries, "dynamicLibraries": dynamicLibraries }
            }];
        }

        prepare: {
            var args = ['rcs', output.filePath];
            for (var i in inputs.obj)
                args.push(inputs.obj[i].filePath);
            var cmd = new Command(ModUtils.moduleProperty(product, "archiverPath"), args);
            cmd.description = 'creating ' + output.fileName;
            cmd.highlight = 'linker'
            cmd.responseFileUsagePrefix = '@';
            return cmd;
        }
    }

    Rule {
        id: loadableModuleLinker
        multiplex: true
        inputs: {
            var tags = ["obj", "linkerscript"];
            if (product.type.contains("loadablemodule") &&
                product.moduleProperty("qbs", "targetOS").contains("darwin") &&
                product.moduleProperty("bundle", "embedInfoPlist"))
                tags.push("aggregate_infoplist");
            return tags;
        }
        inputsFromDependencies: ["dynamiclibrary_copy", "staticlibrary"]

        outputFileTags: ["loadablemodule", "debuginfo_loadablemodule"]
        outputArtifacts: {
            var app = {
                filePath: FileInfo.joinPaths(product.destinationDirectory,
                                             PathTools.loadableModuleFilePath(product)),
                fileTags: ["loadablemodule"]
            }
            return [app].concat(Gcc.debugInfoArtifacts(product, "loadablemodule"));
        }

        prepare: {
            return Gcc.prepareLinker.apply(this, arguments);
        }
    }

    Rule {
        id: applicationLinker
        multiplex: true
        inputs: {
            var tags = ["obj", "linkerscript"];
            if (product.type.contains("application") &&
                product.moduleProperty("qbs", "targetOS").contains("darwin") &&
                product.moduleProperty("bundle", "embedInfoPlist"))
                tags.push("aggregate_infoplist");
            return tags;
        }
        inputsFromDependencies: ["dynamiclibrary_copy", "staticlibrary"]

        outputFileTags: ["application", "debuginfo_app"]
        outputArtifacts: {
            var app = {
                filePath: FileInfo.joinPaths(product.destinationDirectory,
                                             PathTools.applicationFilePath(product)),
                fileTags: ["application"]
            }
            return [app].concat(Gcc.debugInfoArtifacts(product, "app"));
        }

        prepare: {
            return Gcc.prepareLinker.apply(this, arguments);
        }
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c", "objcpp", "objc", "asm_cpp"]
        auxiliaryInputs: ["hpp"]
        explicitlyDependsOn: ["c_pch", "cpp_pch", "objc_pch", "objcpp_pch"]

        outputFileTags: ["obj", "c_obj", "cpp_obj"]
        outputArtifacts: {
            var tags = ["obj"];
            if (inputs.c || inputs.objc)
                tags.push("c_obj");
            if (inputs.cpp || inputs.objcpp)
                tags.push("cpp_obj");
            return [{
                fileTags: tags,
                filePath: FileInfo.joinPaths(".obj",
                                             Utilities.getHash(input.baseDir),
                                             input.fileName + ".o")
            }];
        }

        prepare: {
            return Gcc.prepareCompiler.apply(this, arguments);
        }
    }

    Rule {
        id: assembler
        inputs: ["asm"]

        Artifact {
            fileTags: ["obj"]
            filePath: FileInfo.joinPaths(".obj", Utilities.getHash(input.baseDir), input.fileName + ".o")
        }

        prepare: {
            return Gcc.prepareAssembler.apply(this, arguments);
        }
    }

    Rule {
        condition: useCPrecompiledHeader
        inputs: ["c_pch_src"]
        auxiliaryInputs: ["hpp"]
        Artifact {
            filePath: product.name + "_c.gch"
            fileTags: ["c_pch"]
        }
        prepare: {
            return Gcc.prepareCompiler.apply(this, arguments);
        }
    }

    Rule {
        condition: useCxxPrecompiledHeader
        inputs: ["cpp_pch_src"]
        auxiliaryInputs: ["hpp"]
        Artifact {
            filePath: product.name + "_cpp.gch"
            fileTags: ["cpp_pch"]
        }
        prepare: {
            return Gcc.prepareCompiler.apply(this, arguments);
        }
    }

    Rule {
        condition: useObjcPrecompiledHeader
        inputs: ["objc_pch_src"]
        auxiliaryInputs: ["hpp"]
        Artifact {
            filePath: product.name + "_objc.gch"
            fileTags: ["objc_pch"]
        }
        prepare: {
            return Gcc.prepareCompiler.apply(this, arguments);
        }
    }

    Rule {
        condition: useObjcxxPrecompiledHeader
        inputs: ["objcpp_pch_src"]
        auxiliaryInputs: ["hpp"]
        Artifact {
            filePath: product.name + "_objcpp.gch"
            fileTags: ["objcpp_pch"]
        }
        prepare: {
            return Gcc.prepareCompiler.apply(this, arguments);
        }
    }

    FileTagger {
        patterns: "*.s"
        fileTags: ["asm"]
    }

    FileTagger {
        patterns: "*.S"
        fileTags: ["asm_cpp"]
    }

    FileTagger {
        patterns: "*.sx"
        fileTags: ["asm_cpp"]
    }
}
