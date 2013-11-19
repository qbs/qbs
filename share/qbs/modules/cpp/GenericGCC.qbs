import qbs 1.0
import qbs.FileInfo
import 'windows.js' as Windows
import 'gcc.js' as Gcc
import '../utils.js' as ModUtils
import 'bundle-tools.js' as BundleTools
import 'path-tools.js' as PathTools

CppModule {
    condition: false

    property stringList transitiveSOs
    property string toolchainPrefix
    property path toolchainInstallPath
    compilerName: 'g++'
    linkerName: compilerName
    property string archiverName: 'ar'
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

    readonly property bool shouldCreateSymlinks: {
        return createSymlinks && product.version &&
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
        usings: ["dynamiclibrary", "dynamiclibrary_symlink", "staticlibrary", "frameworkbundle"]

        Artifact {
            fileName: product.destinationDirectory + "/" + PathTools.dynamicLibraryFilePath()
            fileTags: ["dynamiclibrary"]
            cpp.transitiveSOs: {
                var result = []
                for (var i in inputs.dynamiclibrary) {
                    var lib = inputs.dynamiclibrary[i]
                    var impliedLibs = ModUtils.moduleProperties(lib, 'transitiveSOs')
                    var libsToAdd = impliedLibs.concat([lib.fileName]);
                    result = ModUtils.uniqueConcat(result, libsToAdd);
                }
                return result
            }
        }

        // libfoo
        Artifact {
            fileName: product.destinationDirectory + "/" + PathTools.dynamicLibraryFileName(undefined, 0)
            fileTags: ["dynamiclibrary_symlink"]
        }

        // libfoo.1
        Artifact {
            fileName: product.destinationDirectory + "/" + PathTools.dynamicLibraryFileName(undefined, 1)
            fileTags: ["dynamiclibrary_symlink"]
        }

        // libfoo.1.0
        Artifact {
            fileName: product.destinationDirectory + "/" + PathTools.dynamicLibraryFileName(undefined, 2)
            fileTags: ["dynamiclibrary_symlink"]
        }

        prepare: {
            var libFilePath = outputs["dynamiclibrary"][0].fileName;
            var platformLinkerFlags = ModUtils.moduleProperties(product, 'platformLinkerFlags');
            var linkerFlags = ModUtils.moduleProperties(product, 'linkerFlags');
            var commands = [];
            var i;
            var args = Gcc.configFlags(product);
            args.push('-shared');
            if (product.moduleProperty("qbs", "targetOS").contains('linux')) {
                args = args.concat([
                    '-Wl,--hash-style=gnu',
                    '-Wl,--as-needed',
                    '-Wl,--allow-shlib-undefined',
                    '-Wl,--no-undefined',
                    '-Wl,-soname=' + Gcc.soname(product, libFilePath)
                ]);
            } else if (product.moduleProperty("qbs", "targetOS").contains('darwin')) {
                var installNamePrefix = product.moduleProperty("cpp", "installNamePrefix");
                if (installNamePrefix !== undefined)
                    args.push("-Wl,-install_name,"
                              + installNamePrefix + FileInfo.fileName(libFilePath));
                args.push("-Wl,-headerpad_max_install_names");
            }
            args = args.concat(platformLinkerFlags);
            for (i in linkerFlags)
                args.push(linkerFlags[i])
            for (i in inputs.obj)
                args.push(inputs.obj[i].fileName);
            var sysroot = ModUtils.moduleProperty(product, "sysroot")
            if (sysroot) {
                if (product.moduleProperty("qbs", "targetOS").contains('darwin'))
                    args.push('-isysroot', sysroot);
                else
                    args.push('--sysroot=' + sysroot);
            }

            args.push('-o');
            args.push(libFilePath);
            args = args.concat(Gcc.libraryLinkerFlags(product, inputs));
            args = args.concat(Gcc.additionalCompilerAndLinkerFlags(product));
            var cmd = new Command(ModUtils.moduleProperty(product, "linkerPath"), args);
            cmd.description = 'linking ' + FileInfo.fileName(libFilePath);
            cmd.highlight = 'linker';
            cmd.responseFileUsagePrefix = '@';
            commands.push(cmd);

            // Create symlinks from {libfoo, libfoo.1, libfoo.1.0} to libfoo.1.0.0
            if (ModUtils.moduleProperty(product, "shouldCreateSymlinks")) {
                var links = outputs["dynamiclibrary_symlink"];
                var symlinkCount = links.length;
                for (var i = 0; i < symlinkCount; ++i) {
                    cmd = new Command("ln", ["-sf", FileInfo.fileName(libFilePath),
                                                    links[i].fileName]);
                    cmd.highlight = "filegen";
                    cmd.description = "creating symbolic link '"
                            + FileInfo.fileName(links[i].fileName) + "'";
                    cmd.workingDirectory = FileInfo.path(libFilePath);
                    commands.push(cmd);
                }
            }
            return commands;
        }
    }

    Rule {
        id: staticLibraryLinker
        multiplex: true
        inputs: ["obj"]
        usings: ["dynamiclibrary", "staticlibrary", "frameworkbundle"]

        Artifact {
            fileName: product.destinationDirectory + "/" + PathTools.staticLibraryFilePath()
            fileTags: ["staticlibrary"]
            cpp.staticLibraries: {
                var result = []
                for (var i in inputs.staticlibrary) {
                    var lib = inputs.staticlibrary[i]
                    result.push(lib.fileName)
                    var impliedLibs = ModUtils.moduleProperties(lib, 'staticLibraries')
                    result = result.concat(impliedLibs);
                }
                return result
            }
            cpp.dynamicLibraries: {
                var result = []
                for (var i in inputs.dynamiclibrary)
                    result.push(inputs.dynamiclibrary[i].fileName);
                return result
            }
        }

        prepare: {
            var args = ['rcs', output.fileName];
            for (var i in inputs.obj)
                args.push(inputs.obj[i].fileName);
            var cmd = new Command(ModUtils.moduleProperty(product, "archiverPath"), args);
            cmd.description = 'creating ' + FileInfo.fileName(output.fileName);
            cmd.highlight = 'linker'
            cmd.responseFileUsagePrefix = '@';
            return cmd;
        }
    }

    Rule {
        id: applicationLinker
        multiplex: true
        inputs: ["obj"]
        usings: ["dynamiclibrary", "staticlibrary", "frameworkbundle"]

        Artifact {
            fileName: product.destinationDirectory + "/" + PathTools.applicationFilePath()
            fileTags: ["application"]
        }

        prepare: {
            var platformLinkerFlags = ModUtils.moduleProperties(product, 'platformLinkerFlags');
            var linkerFlags = ModUtils.moduleProperties(product, 'linkerFlags');
            var args = Gcc.configFlags(product);
            for (var i in inputs.obj)
                args.push(inputs.obj[i].fileName)
            var sysroot = ModUtils.moduleProperty(product, "sysroot")
            if (sysroot) {
                if (product.moduleProperty("qbs", "targetOS").contains('darwin'))
                    args.push('-isysroot', sysroot)
                else
                    args.push('--sysroot=' + sysroot)
            }
            args = args.concat(platformLinkerFlags);
            for (i in linkerFlags)
                args.push(linkerFlags[i])
            if (product.moduleProperty("qbs", "toolchain").contains("mingw")) {
                if (product.consoleApplication !== undefined)
                    if (product.consoleApplication)
                        args.push("-Wl,-subsystem,console");
                    else
                        args.push("-Wl,-subsystem,windows");

                var minimumWindowsVersion = ModUtils.moduleProperty(product, "minimumWindowsVersion");
                if (minimumWindowsVersion) {
                    var subsystemVersion = Windows.getWindowsVersionInFormat(minimumWindowsVersion, 'subsystem');
                    if (subsystemVersion) {
                        var major = subsystemVersion.split('.')[0];
                        var minor = subsystemVersion.split('.')[1];

                        // http://sourceware.org/binutils/docs/ld/Options.html
                        args.push("-Wl,--major-subsystem-version," + major);
                        args.push("-Wl,--minor-subsystem-version," + minor);
                        args.push("-Wl,--major-os-version," + major);
                        args.push("-Wl,--minor-os-version," + minor);
                    } else {
                        print('WARNING: Unknown Windows version "' + minimumWindowsVersion + '"');
                    }
                }
            }
            args.push('-o');
            args.push(output.fileName);

            if (product.moduleProperty("qbs", "targetOS").contains('linux')) {
                var transitiveSOs = ModUtils.modulePropertiesFromArtifacts(product, inputs.dynamiclibrary, 'cpp', 'transitiveSOs')
                var uniqueSOs = ModUtils.uniqueConcat([], transitiveSOs)
                for (i in uniqueSOs) {
                    args.push("-Wl,-rpath-link=" + FileInfo.path(uniqueSOs[i]))
                }
            }

            args = args.concat(Gcc.libraryLinkerFlags(product, inputs));
            args = args.concat(Gcc.additionalCompilerAndLinkerFlags(product));
            var cmd = new Command(ModUtils.moduleProperty(product, "linkerPath"), args);
            cmd.description = 'linking ' + FileInfo.fileName(output.fileName);
            cmd.highlight = 'linker'
            cmd.responseFileUsagePrefix = '@';
            return cmd;
        }
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c", "objcpp", "objc", "asm", "asm_cpp"]
        auxiliaryInputs: ["hpp"]
        explicitlyDependsOn: ["c_pch", "cpp_pch", "objc_pch", "objcpp_pch"]

        Artifact {
            fileTags: ["obj"]
            fileName: ".obj/" + product.name + "/" + input.baseDir + "/" + input.fileName + ".o"
        }

        prepare: {
            return Gcc.prepareCompiler.apply(this, arguments);
        }
    }

    Transformer {
        condition: cPrecompiledHeader !== undefined
        inputs: cPrecompiledHeader
        Artifact {
            fileName: product.name + "_c.gch"
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
            fileName: product.name + "_cpp.gch"
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
            fileName: product.name + "_objc.gch"
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
            fileName: product.name + "_objcpp.gch"
            fileTags: "objcpp_pch"
        }
        prepare: {
            return Gcc.prepareCompiler.apply(this, arguments);
        }
    }

    FileTagger {
        pattern: "*.s"
        fileTags: ["asm"]
    }

    FileTagger {
        pattern: "*.S"
        fileTags: ["asm_cpp"]
    }

    FileTagger {
        pattern: "*.sx"
        fileTags: ["asm_cpp"]
    }
}
