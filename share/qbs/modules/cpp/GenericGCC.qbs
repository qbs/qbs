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
    compilerName: 'g++'
    linkerName: 'g++'
    property string archiverName: 'ar'
    property string nmName: 'nm'
    property path sysroot: qbs.sysroot
    property path platformPath

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

    compilerPath: toolchainPathPrefix + compilerName
    linkerPath: toolchainPathPrefix + linkerName
    property path archiverPath: { return toolchainPathPrefix + archiverName }
    property path nmPath: { return toolchainPathPrefix + nmName }

    readonly property bool shouldCreateSymlinks: {
        return createSymlinks && internalVersion &&
                !product.type.contains("frameworkbundle") && qbs.targetOS.contains("unix");
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

    Rule {
        id: dynamicLibraryLinker
        multiplex: true
        inputs: ["obj"]
        inputsFromDependencies: ["dynamiclibrary_copy", "staticlibrary", "frameworkbundle"]

        outputFileTags: ["dynamiclibrary", "dynamiclibrary_symlink", "dynamiclibrary_copy"]
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
            if (ModUtils.moduleProperty(product, "shouldCreateSymlinks")) {
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
        inputsFromDependencies: ["dynamiclibrary", "staticlibrary", "frameworkbundle"]

        Artifact {
            filePath: product.destinationDirectory + "/" + PathTools.staticLibraryFilePath(product)
            fileTags: ["staticlibrary"]
            cpp.staticLibraries: {
                var result = []
                for (var i in inputs.staticlibrary) {
                    var lib = inputs.staticlibrary[i]
                    result = Gcc.concatLibs(result, [lib.filePath,
                                                     ModUtils.moduleProperties(lib,
                                                                               'staticLibraries')]);
                }
                return result
            }
            cpp.dynamicLibraries: {
                var result = []
                for (var i in inputs.dynamiclibrary)
                    result.push(inputs.dynamiclibrary[i].filePath);
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
        id: applicationLinker
        multiplex: true
        inputs: {
            var tags = ["obj"];
            if (product.type.contains("application") &&
                product.moduleProperty("qbs", "targetOS").contains("darwin") &&
                product.moduleProperty("cpp", "embedInfoPlist"))
                tags.push("infoplist");
            return tags;
        }
        inputsFromDependencies: ["dynamiclibrary_copy", "staticlibrary", "frameworkbundle"]

        outputFileTags: ["application"]
        outputArtifacts: {
            return [
                {
                    filePath: FileInfo.joinPaths(product.destinationDirectory,
                                                 PathTools.applicationFilePath(product)),
                    fileTags: ["application"]
                }
            ];
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
