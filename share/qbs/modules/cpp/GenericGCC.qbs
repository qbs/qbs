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
    property string toolchainInstallPath
    compilerName: 'g++'
    property string archiverName: 'ar'
    property string sysroot: qbs.sysroot
    property string platformPath

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
    property string archiverPath: { return toolchainPathPrefix + archiverName }

    property bool createSymlinks: true

    readonly property bool shouldCreateSymlinks: {
        return createSymlinks && product.version &&
                !type.contains("frameworkbundle") && qbs.targetOS.contains("unix");
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
                    result.push(lib.fileName)
                    var impliedLibs = ModUtils.moduleProperties(lib, 'transitiveSOs')
                    result = result.concat(impliedLibs)
                }
                return result
            }
        }

        // libfoo
        Artifact {
            condition: product.shouldCreateSymlinks
            fileName: product.destinationDirectory + "/" + PathTools.dynamicLibraryFileName(undefined, 0)
            fileTags: ["dynamiclibrary_symlink"]
        }

        // libfoo.1
        Artifact {
            condition: product.shouldCreateSymlinks
            fileName: product.destinationDirectory + "/" + PathTools.dynamicLibraryFileName(undefined, 1)
            fileTags: ["dynamiclibrary_symlink"]
        }

        // libfoo.1.0
        Artifact {
            condition: product.shouldCreateSymlinks
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
            var cmd = new Command(ModUtils.moduleProperty(product, "compilerPath"), args);
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
                for (i in transitiveSOs) {
                    args.push("-Wl,-rpath-link=" + FileInfo.path(transitiveSOs[i]))
                }
            }

            args = args.concat(Gcc.libraryLinkerFlags(product, inputs));
            args = args.concat(Gcc.additionalCompilerAndLinkerFlags(product));
            var cmd = new Command(ModUtils.moduleProperty(product, "compilerPath"), args);
            cmd.description = 'linking ' + FileInfo.fileName(output.fileName);
            cmd.highlight = 'linker'
            cmd.responseFileUsagePrefix = '@';
            return cmd;
        }
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c", "objcpp", "objc"]
        auxiliaryInputs: ["hpp"]
        explicitlyDependsOn: ["c++_pch"]

        Artifact {
            fileTags: ["obj"]
            fileName: ".obj/" + product.name + "/" + input.baseDir + "/" + input.fileName + ".o"
        }

        prepare: {
            var cFlags = ModUtils.moduleProperties(input, 'cFlags');
            var cxxFlags = ModUtils.moduleProperties(input, 'cxxFlags');
            var objcFlags = ModUtils.moduleProperties(input, 'objcFlags');
            var objcxxFlags = ModUtils.moduleProperties(input, 'objcxxFlags');
            var visibility = ModUtils.moduleProperty(product, 'visibility');
            var args = Gcc.configFlags(input);
            var i, c;

            args.push('-pipe');

            if (!product.type.contains('staticlibrary')
                    && !product.moduleProperty("qbs", "toolchain").contains("mingw")) {
                if (visibility === 'hidden')
                    args.push('-fvisibility=hidden');
                if (visibility === 'hiddenInlines')
                    args.push('-fvisibility-inlines-hidden');
                if (visibility === 'default')
                    args.push('-fvisibility=default')
            }

            var prefixHeaders = ModUtils.moduleProperty(product, "prefixHeaders");
            for (i in prefixHeaders) {
                args.push('-include');
                args.push(prefixHeaders[i]);
            }

            args = args.concat(ModUtils.moduleProperties(input, 'platformCommonCompilerFlags'));
            for (i = 0, c = input.fileTags.length; i < c; ++i) {
                if (input.fileTags[i] === "cpp") {
                    if (ModUtils.moduleProperty(product, "precompiledHeader")) {
                        args.push('-include')
                        args.push(product.name)
                        var pchPath = ModUtils.moduleProperty(product, "precompiledHeaderDir")
                        var pchPathIncluded = false
                        for (var i in includePaths) {
                            if (includePaths[i] == pchPath) {
                                pchPathIncluded = true
                                break
                            }
                        }
                        if (!pchPathIncluded)
                            args.push('-I' + pchPath)
                    }
                    args = args.concat(
                                ModUtils.moduleProperties(input, 'platformCxxFlags'),
                                cxxFlags);
                    break;
                } else if (input.fileTags[i] === "c") {
                    args.push('-x');
                    args.push('c');
                    args = args.concat(
                                ModUtils.moduleProperties(input, 'platformCFlags'),
                                cFlags);
                    break;
                } else if (input.fileTags[i] === "objc") {
                    args.push('-x');
                    args.push('objective-c');
                    args = args.concat(
                                ModUtils.moduleProperties(input, 'platformObjcFlags'),
                                objcFlags);
                    break;
                } else if (input.fileTags[i] === "objcpp") {
                    args.push('-x');
                    args.push('objective-c++');
                    args = args.concat(
                                ModUtils.moduleProperties(input, 'platformObjcxxFlags'),
                                objcxxFlags);
                    break;
                }
            }
            args = args.concat(ModUtils.moduleProperties(input, 'commonCompilerFlags'));
            args = args.concat(Gcc.additionalCompilerFlags(product, input, output));
            args = args.concat(Gcc.additionalCompilerAndLinkerFlags(product));
            var compilerPath = ModUtils.moduleProperty(product, "compilerPath");
            var wrapperArgs = ModUtils.moduleProperty(product, "compilerWrapper");
            if (wrapperArgs && wrapperArgs.length > 0) {
                args.unshift(compilerPath);
                compilerPath = wrapperArgs.shift();
                args = wrapperArgs.concat(args);
            }
            var cmd = new Command(compilerPath, args);
            cmd.description = 'compiling ' + FileInfo.fileName(input.fileName);
            cmd.highlight = "compiler";
            cmd.responseFileUsagePrefix = '@';
            return cmd;
        }
    }

    Transformer {
        condition: precompiledHeader != null
        inputs: precompiledHeader
        Artifact {
            fileName: product.name + ".gch"
            fileTags: "c++_pch"
        }
        prepare: {
            var args = Gcc.configFlags(product);
            args.push('-x');
            args.push('c++-header');
            var cxxFlags = ModUtils.moduleProperty(product, "cxxFlags")
            if (cxxFlags)
                args = args.concat(cxxFlags);
            args = args.concat(Gcc.additionalCompilerFlags(product, input, output));
            args = args.concat(Gcc.additionalCompilerAndLinkerFlags(product));
            var cmd = new Command(ModUtils.moduleProperty(product, "compilerPath"), args);
            cmd.description = 'precompiling ' + FileInfo.fileName(input.fileName);
            cmd.responseFileUsagePrefix = '@';
            return cmd;
        }
    }
}
