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
        usings: ["dynamiclibrary_copy", "staticlibrary", "frameworkbundle"]

        Artifact {
            filePath: product.destinationDirectory + "/" + PathTools.dynamicLibraryFilePath(product)
            fileTags: ["dynamiclibrary"]
        }

        // libfoo
        Artifact {
            filePath: product.destinationDirectory + "/" + PathTools.dynamicLibraryFileName(product, undefined, 0)
            fileTags: ["dynamiclibrary_symlink"]
        }

        // libfoo.1
        Artifact {
            filePath: product.destinationDirectory + "/" + PathTools.dynamicLibraryFileName(product, undefined, 1)
            fileTags: ["dynamiclibrary_symlink"]
        }

        // libfoo.1.0
        Artifact {
            filePath: product.destinationDirectory + "/" + PathTools.dynamicLibraryFileName(product, undefined, 2)
            fileTags: ["dynamiclibrary_symlink"]
        }

        // Copy of dynamic lib for smart re-linking.
        Artifact {
            filePath: product.destinationDirectory + "/.socopy/"
                      + PathTools.dynamicLibraryFilePath(product)
            fileTags: ["dynamiclibrary_copy"]
            alwaysUpdated: false
            cpp.transitiveSOs: {
                var result = []
                for (var i in inputs.dynamiclibrary_copy) {
                    var lib = inputs.dynamiclibrary_copy[i]
                    var impliedLibs = ModUtils.moduleProperties(lib, 'transitiveSOs')
                    var libsToAdd = [lib.filePath].concat(impliedLibs);
                    result = result.concat(libsToAdd);
                }
                result = Gcc.concatLibs([], result);
                return result;
            }
        }

        prepare: {
            // Actual linker command.
            var lib = outputs["dynamiclibrary"][0];
            var platformLinkerFlags = ModUtils.moduleProperties(product, 'platformLinkerFlags');
            var linkerFlags = ModUtils.moduleProperties(product, 'linkerFlags');
            var commands = [];
            var i;
            var args = Gcc.configFlags(product);
            args.push('-shared');
            if (product.moduleProperty("qbs", "targetOS").contains('linux')) {
                args = args.concat([
                    '-Wl,--as-needed',
                    '-Wl,-soname=' + UnixUtils.soname(product, lib.fileName)
                ]);
            } else if (product.moduleProperty("qbs", "targetOS").contains('darwin')) {
                var installNamePrefix = product.moduleProperty("cpp", "installNamePrefix");
                if (installNamePrefix !== undefined)
                    args.push("-Wl,-install_name,"
                              + installNamePrefix + lib.fileName);
                args.push("-Wl,-headerpad_max_install_names");
            }
            args = args.concat(platformLinkerFlags);
            for (i in linkerFlags)
                args.push(linkerFlags[i])
            for (i in inputs.obj)
                args.push(inputs.obj[i].filePath);
            var sysroot = ModUtils.moduleProperty(product, "sysroot")
            if (sysroot) {
                if (product.moduleProperty("qbs", "targetOS").contains('darwin'))
                    args.push('-isysroot', sysroot);
                else
                    args.push('--sysroot=' + sysroot);
            }
            if (product.moduleProperty("cpp", "entryPoint"))
                args.push("-Wl,-e", product.moduleProperty("cpp", "entryPoint"));

            args.push('-o');
            args.push(lib.filePath);
            args = args.concat(Gcc.linkerFlags(product, inputs));
            args = args.concat(Gcc.additionalCompilerAndLinkerFlags(product));
            var cmd = new Command(ModUtils.moduleProperty(product, "linkerPath"), args);
            cmd.description = 'linking ' + lib.fileName;
            cmd.highlight = 'linker';
            cmd.responseFileUsagePrefix = '@';
            commands.push(cmd);

            // Update the copy, if any global symbols have changed.
            cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                var sourceFilePath = outputs.dynamiclibrary[0].filePath;
                var targetFilePath = outputs.dynamiclibrary_copy[0].filePath;
                if (!File.exists(targetFilePath)) {
                    File.copy(sourceFilePath, targetFilePath);
                    return;
                }
                var process = new Process();
                var command = ModUtils.moduleProperty(product, "nmPath");
                var args = ["-g", "-D", "-P"];
                if (process.exec(command, args.concat(sourceFilePath), false) != 0) {
                    // Failure to run the nm tool is not fatal. We just fall back to the
                    // "always relink" behavior.
                    File.copy(sourceFilePath, targetFilePath);
                    return;
                }
                var globalSymbolsSource = process.readStdOut();
                if (process.exec(command, args.concat(targetFilePath), false) != 0) {
                    File.copy(sourceFilePath, targetFilePath);
                    return;
                }
                var globalSymbolsTarget = process.readStdOut();
                process.close();

                var globalSymbolsSourceLines = globalSymbolsSource.split('\n');
                var globalSymbolsTargetLines = globalSymbolsTarget.split('\n');
                if (globalSymbolsSourceLines.length !== globalSymbolsTargetLines.length) {
                    File.copy(sourceFilePath, targetFilePath);
                    return;
                }
                while (globalSymbolsSourceLines.length > 0) {
                    var sourceLine = globalSymbolsSourceLines.shift();
                    var targetLine = globalSymbolsTargetLines.shift();
                    var sourceLineElems = sourceLine.split(/\s+/);
                    var targetLineElems = targetLine.split(/\s+/);
                    if (sourceLineElems[0] !== targetLineElems[0] // Object name.
                            || sourceLineElems[1] !== targetLineElems[1]) { // Object type
                        File.copy(sourceFilePath, targetFilePath);
                        return;
                    }
                }
            }
            commands.push(cmd);

            // Create symlinks from {libfoo, libfoo.1, libfoo.1.0} to libfoo.1.0.0
            if (ModUtils.moduleProperty(product, "shouldCreateSymlinks")) {
                var links = outputs["dynamiclibrary_symlink"];
                var symlinkCount = links.length;
                for (var i = 0; i < symlinkCount; ++i) {
                    cmd = new Command("ln", ["-sf", lib.fileName,
                                                    links[i].filePath]);
                    cmd.highlight = "filegen";
                    cmd.description = "creating symbolic link '"
                            + links[i].fileName + "'";
                    cmd.workingDirectory = FileInfo.path(lib.filePath);
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
        usings: ["dynamiclibrary_copy", "staticlibrary", "frameworkbundle"]

        Artifact {
            filePath: product.destinationDirectory + "/" + PathTools.applicationFilePath(product)
            fileTags: ["application"]
        }

        prepare: {
            var platformLinkerFlags = ModUtils.moduleProperties(product, 'platformLinkerFlags');
            var linkerFlags = ModUtils.moduleProperties(product, 'linkerFlags');
            var args = Gcc.configFlags(product);
            for (var i in inputs.obj)
                args.push(inputs.obj[i].filePath)
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
                    var subsystemVersion = WindowsUtils.getWindowsVersionInFormat(minimumWindowsVersion, 'subsystem');
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
            args.push(output.filePath);

            if (inputs.infoplist) {
                args = args.concat(["-sectcreate", "__TEXT", "__info_plist", inputs.infoplist[0].filePath]);
            }

            args = args.concat(Gcc.linkerFlags(product, inputs));
            args = args.concat(Gcc.additionalCompilerAndLinkerFlags(product));
            var cmd = new Command(ModUtils.moduleProperty(product, "linkerPath"), args);
            cmd.description = 'linking ' + output.fileName;
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
            filePath: ".obj/" + input.baseDir + "/" + input.fileName + ".o"
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
