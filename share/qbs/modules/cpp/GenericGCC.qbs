/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
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
import qbs.Process
import qbs.UnixUtils
import qbs.WindowsUtils
import 'gcc.js' as Gcc

CppModule {
    condition: false

    cxxStandardLibrary: {
        if (cxxLanguageVersion && qbs.toolchain.contains("clang")) {
            return cxxLanguageVersion !== "c++98" ? "libc++" : "libstdc++";
        }
    }

    property stringList transitiveSOs
    property string toolchainPrefix
    property path toolchainInstallPath
    compilerName: cxxCompilerName
    linkerName: cxxCompilerName
    property string archiverName: 'ar'
    property string nmName: 'nm'
    property string objcopyName: "objcopy"
    property string stripName: "strip"
    property string dsymutilName: "dsymutil"
    property path sysroot: qbs.sysroot
    property path platformPath

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

    property string cCompilerName: executablePrefix
                                   + (qbs.toolchain.contains("clang") ? "clang" : "gcc")
                                   + executableSuffix
    property string cxxCompilerName: executablePrefix
                                     + (qbs.toolchain.contains("clang") ? "clang++" : "g++")
                                     + executableSuffix

    compilerPathByLanguage: {
        return {
            "c": toolchainPathPrefix + cCompilerName,
            "cpp": toolchainPathPrefix + cxxCompilerName,
            "objc": toolchainPathPrefix + cCompilerName,
            "objcpp": toolchainPathPrefix + cxxCompilerName
        };
    }

    compilerPath: toolchainPathPrefix + compilerName
    linkerPath: toolchainPathPrefix + linkerName
    property path archiverPath: { return toolchainPathPrefix + archiverName }
    property path nmPath: { return toolchainPathPrefix + nmName }
    property string objcopyPath: toolchainPathPrefix + objcopyName
    property string stripPath: toolchainPathPrefix + stripName
    property string dsymutilPath: toolchainPathPrefix + dsymutilName
    property stringList dsymutilFlags

    readonly property bool shouldCreateSymlinks: {
        return createSymlinks && internalVersion && qbs.targetOS.contains("unix");
    }

    readonly property string internalVersion: {
        if (product.version === undefined)
            return undefined;

        if (typeof product.version !== "string"
                || !product.version.match(/^([0-9]+\.){0,3}[0-9]+$/))
            throw("product.version must be a string in the format x[.y[.z[.w]] "
                + "where each component is an integer");

        var maxVersionParts = 3;
        var versionParts = product.version.split('.').slice(0, maxVersionParts);

        // pad if necessary
        for (var i = versionParts.length; i < maxVersionParts; ++i)
            versionParts.push("0");

        return versionParts.join('.');
    }

    validate: {
        var validator = new ModUtils.PropertyValidator("cpp");
        validator.setRequiredProperty("architecture", architecture,
                                      "you might want to re-run 'qbs-setup-toolchains'");
        validator.validate();
    }

    Rule {
        id: dynamicLibraryLinker
        multiplex: true
        inputs: ["obj"]
        inputsFromDependencies: ["dynamiclibrary_copy", "staticlibrary"]

        outputFileTags: ["dynamiclibrary", "dynamiclibrary_symlink", "dynamiclibrary_copy", "debuginfo"]
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
                cpp: { transitiveSOs: Gcc.collectTransitiveSos(inputs) }
            };
            var artifacts = [lib, libCopy];

            if (ModUtils.moduleProperty(product, "shouldCreateSymlinks") && !product.moduleProperty("bundle", "isBundle")) {
                for (var i = 0; i < 3; ++i) {
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
            if (ModUtils.moduleProperty(product, "separateDebugInformation")) {
                artifacts.push({
                    filePath: FileInfo.joinPaths(product.destinationDirectory, PathTools.debugInfoFileName(product)),
                    fileTags: ["debuginfo"]
                });
            }
            return artifacts;
        }

        prepare: {
            return Gcc.prepareLinker.apply(this, arguments);
        }
    }

    Rule {
        id: staticLibraryLinker
        multiplex: true
        inputs: ["obj"]
        inputsFromDependencies: ["dynamiclibrary", "staticlibrary"]

        Artifact {
            filePath: product.destinationDirectory + "/" + PathTools.staticLibraryFilePath(product)
            fileTags: ["staticlibrary"]
            cpp.staticLibraries: {
                var result = [];
                for (var i in inputs.staticlibrary) {
                    var lib = inputs.staticlibrary[i]
                    result = Gcc.concatLibs(result, [lib.filePath].concat(
                                            ModUtils.moduleProperties(lib, 'staticLibraries')));
                }
                result = Gcc.concatLibs(result,
                                        ModUtils.moduleProperties(product, 'staticLibraries'));
                return result
            }
            cpp.dynamicLibraries: {
                var result = [];
                for (var i in inputs.dynamiclibrary)
                    result.push(inputs.dynamiclibrary[i].filePath);
                result = result.concat(ModUtils.moduleProperties(product, 'dynamicLibraries'));
                return result
            }
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
            var tags = ["obj"];
            if (product.type.contains("application") &&
                product.moduleProperty("qbs", "targetOS").contains("darwin") &&
                product.moduleProperty("bundle", "embedInfoPlist"))
                tags.push("infoplist");
            return tags;
        }
        inputsFromDependencies: ["dynamiclibrary_copy", "framework_copy", "staticlibrary"]

        outputFileTags: ["loadablemodule", "debuginfo", "dsym"]
        outputArtifacts: {
            var app = {
                filePath: FileInfo.joinPaths(product.destinationDirectory,
                                             PathTools.loadableModuleFilePath(product)),
                fileTags: ["loadablemodule"]
            }
            var artifacts = [app];
            if (!product.moduleProperty("qbs", "targetOS").contains("darwin")
                    && ModUtils.moduleProperty(product, "separateDebugInformation")) {
                artifacts.push({
                    filePath: app.filePath + ".debug",
                    fileTags: ["debuginfo"]
                });
            }
            if (ModUtils.moduleProperty(product, "buildDsym")) {
                artifacts.push({
                    filePath: FileInfo.joinPaths(product.destinationDirectory, PathTools.dwarfDsymFileName(product)),
                    fileTags: ["dsym"]
                });
            }
            return artifacts;
        }

        prepare: {
            return Gcc.prepareLinker.apply(this, arguments);
        }
    }

    Rule {
        id: applicationLinker
        multiplex: true
        inputs: {
            var tags = ["obj"];
            if (product.type.contains("application") &&
                product.moduleProperty("qbs", "targetOS").contains("darwin") &&
                product.moduleProperty("bundle", "embedInfoPlist"))
                tags.push("infoplist");
            return tags;
        }
        inputsFromDependencies: ["dynamiclibrary_copy", "staticlibrary"]

        outputFileTags: ["application", "debuginfo"]
        outputArtifacts: {
            var app = {
                filePath: FileInfo.joinPaths(product.destinationDirectory,
                                             PathTools.applicationFilePath(product)),
                fileTags: ["application"]
            }
            var artifacts = [app];
            if (ModUtils.moduleProperty(product, "separateDebugInformation")) {
                artifacts.push({
                    filePath: FileInfo.joinPaths(product.destinationDirectory, PathTools.debugInfoFileName(product)),
                    fileTags: ["debuginfo"]
                });
            }
            return artifacts;
        }

        prepare: {
            return Gcc.prepareLinker.apply(this, arguments);
        }
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c", "objcpp", "objc", "asm", "asm_cpp"]
        auxiliaryInputs: ["hpp"]
        explicitlyDependsOn: ["c_pch", "cpp_pch", "objc_pch", "objcpp_pch"]

        Artifact {
            fileTags: ["obj"]
            filePath: ".obj/" + qbs.getHash(input.baseDir) + "/" + input.fileName + ".o"
        }

        prepare: {
            return Gcc.prepareCompiler.apply(this, arguments);
        }
    }

    Transformer {
        condition: cPrecompiledHeader !== undefined
        inputs: cPrecompiledHeader
        Artifact {
            filePath: product.name + "_c.gch"
            fileTags: "c_pch"
        }
        prepare: {
            return Gcc.prepareCompiler.apply(this, arguments);
        }
    }

    Transformer {
        condition: cxxPrecompiledHeader !== undefined
        inputs: cxxPrecompiledHeader
        Artifact {
            filePath: product.name + "_cpp.gch"
            fileTags: "cpp_pch"
        }
        prepare: {
            return Gcc.prepareCompiler.apply(this, arguments);
        }
    }

    Transformer {
        condition: objcPrecompiledHeader !== undefined
        inputs: objcPrecompiledHeader
        Artifact {
            filePath: product.name + "_objc.gch"
            fileTags: "objc_pch"
        }
        prepare: {
            return Gcc.prepareCompiler.apply(this, arguments);
        }
    }

    Transformer {
        condition: objcxxPrecompiledHeader !== undefined
        inputs: objcxxPrecompiledHeader
        Artifact {
            filePath: product.name + "_objcpp.gch"
            fileTags: "objcpp_pch"
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
