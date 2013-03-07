import qbs 1.0
import qbs.fileinfo as FileInfo
import 'gcc.js' as Gcc
import '../utils.js' as ModUtils

CppModule {
    condition: false

    property var transitiveSOs
    property string toolchainPrefix
    property string toolchainInstallPath
    property string compilerName: 'g++'
    property string archiverName: 'ar'
    property string sysroot: qbs.sysroot

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

    property string compilerPath: { return toolchainPathPrefix + compilerName }
    property string archiverPath: { return toolchainPathPrefix + archiverName }

    Rule {
        id: dynamicLibraryLinker
        multiplex: true
        inputs: ["obj"]
        usings: ['dynamiclibrary', 'staticlibrary']

        Artifact {
            fileName: product.destinationDirectory + "/"
                      + ModUtils.moduleProperty(product, "dynamicLibraryPrefix") + product.targetName
                      + ModUtils.moduleProperty(product, "dynamicLibrarySuffix")
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

        prepare: {
            var libraryPaths = ModUtils.moduleProperties(product, 'libraryPaths');
            var dynamicLibraries = ModUtils.moduleProperties(product, 'dynamicLibraries');
            var staticLibraries = ModUtils.moduleProperties(product, 'staticLibraries');
            var frameworkPaths = ModUtils.moduleProperties(product, 'frameworkPaths');
            var frameworks = ModUtils.modulePropertiesFromArtifacts(product, inputs.dynamiclibrary, 'cpp', 'frameworks');
            var weakFrameworks = ModUtils.modulePropertiesFromArtifacts(product, inputs.dynamiclibrary, 'cpp', 'weakFrameworks');
            var rpaths = ModUtils.moduleProperties(product, 'rpaths');
            var linkerFlags = ModUtils.moduleProperties(product, 'linkerFlags');
            var i;
            var args = Gcc.configFlags(product);
            args.push('-shared');
            if (product.moduleProperty("qbs", "targetOS") === 'linux')
                args = args.concat([
                    '-Wl,--hash-style=gnu',
                    '-Wl,--as-needed',
                    '-Wl,--allow-shlib-undefined',
                    '-Wl,--no-undefined',
                    '-Wl,-soname=' + FileInfo.fileName(output.fileName)
                ]);
            for (i in linkerFlags)
                args.push(linkerFlags[i])
            for (i in inputs.obj)
                args.push(inputs.obj[i].fileName);
            var sysroot = ModUtils.moduleProperty(product, "sysroot")
            if (sysroot)
                args.push('--sysroot=' + sysroot)
            var staticLibrariesI = [];
            for (i in inputs.staticlibrary) {
                staticLibrariesI.push(inputs.staticlibrary[i].fileName);
            }
            staticLibrariesI = staticLibrariesI.concat(staticLibraries);

            var frameworksI = frameworks;
            for (i in inputs.framework) {
                fileName = inputs.framework[i].fileName;
                frameworkPaths.push(FileInfo.path(fileName));
                fileName = Gcc.removePrefixAndSuffix(FileInfo.fileName(fileName),
                                                     ModUtils.moduleProperty(product, "dynamicLibraryPrefix"),
                                                     ModUtils.moduleProperty(product, "dynamicLibrarySuffix"));
                frameworksI.push(fileName);
            }

            var weakFrameworksI = weakFrameworks;

            args.push('-o');
            args.push(output.fileName);
            args = args.concat(Gcc.libs(libraryPaths, frameworkPaths, rpaths, dynamicLibraries, staticLibrariesI, frameworksI, weakFrameworksI));
            for (i in inputs.dynamiclibrary)
                args.push(inputs.dynamiclibrary[i].fileName);
            var cmd = new Command(ModUtils.moduleProperty(product, "compilerPath"), args);
            cmd.description = 'linking ' + FileInfo.fileName(output.fileName);
            cmd.highlight = 'linker';
            return cmd;
        }
    }

    Rule {
        id: staticLibraryLinker
        multiplex: true
        inputs: ["obj"]
        usings: ['dynamiclibrary', 'staticlibrary']

        Artifact {
            fileName: product.destinationDirectory + "/" + ModUtils.moduleProperty(product, "staticLibraryPrefix")
                      + product.targetName + ModUtils.moduleProperty(product, "staticLibrarySuffix")
            fileTags: ["staticlibrary"]
            cpp.staticLibraries: {
                var result = []
                for (var i in inputs.staticlibrary) {
                    var lib = inputs.staticlibrary[i]
                    result.push(lib.fileName)
                    var impliedLibs = ModUtils.moduleProperties(lib, 'staticLibraries')
                    result.concat(impliedLibs)
                }
                return result
            }
            cpp.dynamicLibraries: {
                var result = []
                for (var i in inputs.dynamiclibrary) {
                    var lib = inputs.dynamiclibrary[i]
                    result.push(lib.fileName)
                    var impliedLibs = ModUtils.moduleProperties(lib, 'dynamicLibraries')
                    result.concat(impliedLibs)
                }
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
            return cmd;
        }
    }

    Rule {
        id: applicationLinker
        multiplex: true
        inputs: ["obj"]
        usings: ['dynamiclibrary', 'staticlibrary']

        Artifact {
            fileName: product.destinationDirectory + "/" + ModUtils.moduleProperty(product, "executablePrefix")
                      + product.targetName + ModUtils.moduleProperty(product, "executableSuffix")
            fileTags: ["application"]
        }

        prepare: {
            var libraryPaths = ModUtils.moduleProperties(product, 'libraryPaths');
            var dynamicLibraries = ModUtils.modulePropertiesFromArtifacts(product, inputs.dynamiclibrary, 'cpp', 'dynamicLibraries');
            var staticLibraries = ModUtils.modulePropertiesFromArtifacts(product, inputs.staticlibrary, 'cpp', 'staticLibraries');
            var frameworkPaths = ModUtils.moduleProperties(product, 'frameworkPaths');
            var frameworks = ModUtils.modulePropertiesFromArtifacts(product, inputs.dynamiclibrary, 'cpp', 'frameworks');
            var weakFrameworks = ModUtils.modulePropertiesFromArtifacts(product, inputs.dynamiclibrary, 'cpp', 'weakFrameworks');
            var rpaths = ModUtils.moduleProperties(product, 'rpaths');
            var linkerFlags = ModUtils.moduleProperties(product, 'linkerFlags');
            var args = Gcc.configFlags(product);
            for (var i in inputs.obj)
                args.push(inputs.obj[i].fileName)
            var sysroot = ModUtils.moduleProperty(product, "sysroot")
            if (sysroot)
                args.push('--sysroot=' + sysroot)
            for (i in linkerFlags)
                args.push(linkerFlags[i])
            if (product.consoleApplication !== undefined
                    && product.moduleProperty("qbs", "toolchain") === "mingw") {
                if (product.consoleApplication)
                    args.push("-Wl,-subsystem,console");
                else
                    args.push("-Wl,-subsystem,windows");
            }
            args.push('-o');
            args.push(output.fileName);

            var staticLibrariesI = [];
            for (i in inputs.staticlibrary) {
                staticLibrariesI.push(inputs.staticlibrary[i].fileName);
            }
            staticLibrariesI = staticLibrariesI.concat(staticLibraries);

            var dynamicLibrariesI = [];
            var dllPrefix = ModUtils.moduleProperty(product, "dynamicLibraryPrefix")
            var dllSuffix = ModUtils.moduleProperty(product, "dynamicLibrarySuffix")
            for (i in dynamicLibraries) {
                if (dynamicLibraries[i].match("^" + dllPrefix + ".*\\" + dllSuffix + "$") !== null) {
                    // shared object filename found
                    var libDir = FileInfo.path(dynamicLibraries[i])
                    var libName = FileInfo.fileName(dynamicLibraries[i])
                    libName = Gcc.removePrefixAndSuffix(libName, dllPrefix, dllSuffix);
                    libraryPaths.push(libDir)
                    dynamicLibrariesI.push(libName)
                } else {
                    // shared object libname found
                    dynamicLibrariesI.push(dynamicLibraries[i])
                }
            }

            if (product.moduleProperty("qbs", "targetOS") === 'linux') {
                var transitiveSOs = ModUtils.modulePropertiesFromArtifacts(product, inputs.dynamiclibrary, 'cpp', 'transitiveSOs')
                for (i in transitiveSOs) {
                    args.push("-Wl,-rpath-link=" + FileInfo.path(transitiveSOs[i]))
                }
            }

            var frameworksI = frameworks;
            for (i in inputs.framework) {
                fileName = inputs.framework[i].fileName;
                frameworkPaths.push(FileInfo.path(fileName));
                fileName = Gcc.removePrefixAndSuffix(FileInfo.fileName(fileName), dllPrefix, dllSuffix);
                frameworksI.push(fileName);
            }

            var weakFrameworksI = weakFrameworks;

            args = args.concat(Gcc.libs(libraryPaths, frameworkPaths, rpaths, dynamicLibrariesI, staticLibrariesI, frameworksI, weakFrameworksI));
            for (i in inputs.dynamiclibrary)
                args.push(inputs.dynamiclibrary[i].fileName);
            var cmd = new Command(ModUtils.moduleProperty(product, "compilerPath"), args);
            cmd.description = 'linking ' + FileInfo.fileName(output.fileName);
            cmd.highlight = 'linker'
            return cmd;
        }
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c", "objcpp", "objc"]
        explicitlyDependsOn: ["c++_pch"]

        Artifact {
            fileTags: ["obj"]
            // ### make it possible to override ".obj" in a project file
            fileName: ".obj/" + product.name + "/" + input.baseDir + "/" + input.completeBaseName + ".o"
        }

        prepare: {
            var includePaths = ModUtils.moduleProperties(input, 'includePaths');
            var frameworkPaths = ModUtils.moduleProperties(product, 'frameworkPaths');
            var systemIncludePaths = ModUtils.moduleProperties(input, 'systemIncludePaths');
            var cFlags = ModUtils.moduleProperties(input, 'cFlags');
            var cxxFlags = ModUtils.moduleProperties(input, 'cxxFlags');
            var objcFlags = ModUtils.moduleProperties(input, 'objcFlags');
            var visibility = ModUtils.moduleProperty(product, 'visibility');
            var args = Gcc.configFlags(input);
            var isCxx = true;
            var isObjC = false;

            args.push('-pipe');

            if (product.type.indexOf('staticlibrary') === -1
                    && product.moduleProperty("qbs", "toolchain") !== "mingw") {
                if (visibility === 'hidden')
                    args.push('-fvisibility=hidden');
                if (visibility === 'hiddenInlines')
                    args.push('-fvisibility-inlines-hidden');
                if (visibility === 'default')
                    args.push('-fvisibility=default')
            }

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
            if (input.fileTags.indexOf("c") >= 0) {
                isCxx = false;
                isObjC = false;
                args.push('-x')
                args.push('c')
            } else if (input.fileTags.indexOf("objc") >= 0) {
                isObjC = true;
                isCxx = false;
                args.push('-x');
                args.push('objective-c');
            } else if (input.fileTags.indexOf("objcpp") >= 0) {
                isObjC = true;
                isCxx = true;
                args.push('-x');
                args.push('objective-c++');
            }
            if (isObjC) {
                if (objcFlags)
                    args = args.concat(objcFlags);
            } else if (isCxx) {
                if (cxxFlags)
                    args = args.concat(cxxFlags);
            } else {
                if (cFlags)
                    args = args.concat(cFlags);
            }
            args = args.concat(Gcc.additionalFlags(product, includePaths, frameworkPaths, systemIncludePaths, input.fileName, output))
            var cmd = new Command(ModUtils.moduleProperty(product, "compilerPath"), args);
            cmd.description = 'compiling ' + FileInfo.fileName(input.fileName);
            cmd.highlight = "compiler";
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
            var includePaths = ModUtils.moduleProperties(input, 'includePaths');
            var frameworkPaths = ModUtils.moduleProperties(product, 'frameworkPaths');
            var systemIncludePaths = ModUtils.moduleProperties(input, 'systemIncludePaths');
            args.push('-x');
            args.push('c++-header');
            var cxxFlags = ModUtils.moduleProperty(product, "cxxFlags")
            if (cxxFlags)
                args = args.concat(cxxFlags);
            args = args.concat(Gcc.additionalFlags(product, includePaths, frameworkPaths, systemIncludePaths,
                    ModUtils.moduleProperty(product, "precompiledHeader"), output));
            var cmd = new Command(ModUtils.moduleProperty(product, "compilerPath"), args);
            cmd.description = 'precompiling ' + FileInfo.fileName(input.fileName);
            return cmd;
        }
    }
}
