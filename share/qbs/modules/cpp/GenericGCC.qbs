import qbs.base 1.0
import qbs.fileinfo 1.0 as FileInfo
import 'gcc.js' as Gcc
import '../utils.js' as ModUtils

CppModule {
    condition: false

    property var transitiveSOs
    property string toolchainPrefix
    property string toolchainInstallPath
    property string compilerName: 'g++'
    property string sysroot: qbs.sysroot

    property string compilerPath: {
        var path = ''
        if (toolchainInstallPath) {
            path += toolchainInstallPath
            if (path.substr(-1) !== '/')
                path += '/'
        }
        if (toolchainPrefix)
            path += toolchainPrefix
        path += compilerName
        return path
    }

    Rule {
        id: dynamicLibraryLinker
        multiplex: true
        inputs: ["obj"]
        usings: ['dynamiclibrary', 'staticlibrary']

        Artifact {
            fileName: product.destinationDirectory + "/" + product.module.dynamicLibraryPrefix + product.name + product.module.dynamicLibrarySuffix
            fileTags: ["dynamiclibrary"]
            cpp.transitiveSOs: {
                var result = []
                for (var i in inputs.dynamiclibrary) {
                    var lib = inputs.dynamiclibrary[i]
                    result.push(lib.fileName)
                    var impliedLibs = ModUtils.appendAll(lib, 'transitiveSOs')
                    result = result.concat(impliedLibs)
                }
                return result
            }
        }

        TransformProperties {
            property var libraryPaths: ModUtils.appendAll(product, 'libraryPaths')
            property var dynamicLibraries: ModUtils.appendAll(product, 'dynamicLibraries')
            property var staticLibraries: ModUtils.appendAll(product, 'staticLibraries')
            property var rpaths: ModUtils.appendAll(product, 'rpaths')
        }

        prepare: {
            var args = Gcc.configFlags(product);
            if (product.modules.qbs.targetOS == 'linux')
                args = args.concat([
                    '-Wl,--hash-style=gnu',
                    '-Wl,--as-needed',
                    '-Wl,--allow-shlib-undefined',
                    '-Wl,--no-undefined',
                    '-Wl,-soname=' + FileInfo.fileName(output.fileName)
                ]);
            args = args.concat([
                '-Wl,-rpath,\$ORIGIN',
                '-shared'
            ]);
            for (var i in inputs.obj)
                args.push(inputs.obj[i].fileName);
            if (product.module.sysroot)
                args.push('--sysroot=' + product.module.sysroot)
            var staticLibrariesI = [];
            for (i in inputs.staticlibrary) {
                staticLibrariesI.push(inputs.staticlibrary[i].fileName);
            }
            staticLibrariesI = staticLibrariesI.concat(staticLibraries);

            var dynamicLibrariesI = [];
            for (i in inputs.dynamiclibrary) {
                libraryPaths.push(FileInfo.path(inputs.dynamiclibrary[i].fileName))
                var fileName = FileInfo.fileName(inputs.dynamiclibrary[i].fileName)
                fileName = fileName.substr(3, fileName.length - 6)
                dynamicLibrariesI.push(fileName)
            }
            dynamicLibrariesI = dynamicLibrariesI.concat(dynamicLibraries);

            args.push('-o');
            args.push(output.fileName);
            args = args.concat(Gcc.libs(libraryPaths, rpaths, dynamicLibrariesI, staticLibrariesI));
            var cmd = new Command(product.module.compilerPath, args);
            Gcc.linkEnv(cmd)
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
            fileName: product.destinationDirectory + "/" + product.module.staticLibraryPrefix + product.name + product.module.staticLibrarySuffix
            fileTags: ["staticlibrary"]
            cpp.staticLibraries: {
                var result = []
                for (var i in inputs.staticlibrary) {
                    var lib = inputs.staticlibrary[i]
                    result.push(lib.fileName)
                    var impliedLibs = ModUtils.appendAll(lib, 'staticLibraries')
                    result.concat(impliedLibs)
                }
                return result
            }
            cpp.dynamicLibraries: {
                var result = []
                for (var i in inputs.dynamiclibrary) {
                    var lib = inputs.dynamiclibrary[i]
                    result.push(lib.fileName)
                    var impliedLibs = ModUtils.appendAll(lib, 'dynamicLibraries')
                    result.concat(impliedLibs)
                }
                return result
            }
        }

        prepare: {
            var args = ['rcs', output.fileName];
            for (var i in inputs.obj)
                args.push(inputs.obj[i].fileName);
            var cmd = new Command('ar', args);
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
            fileName: product.destinationDirectory + "/" + product.module.executablePrefix + product.name + product.module.executableSuffix
            fileTags: ["application"]
        }

        TransformProperties {
            property var libraryPaths: ModUtils.appendAll(product, 'libraryPaths')
            property var dynamicLibraries: ModUtils.appendAllFromArtifacts(product, inputs.dynamiclibrary, 'cpp', 'dynamicLibraries')
            property var staticLibraries: ModUtils.appendAllFromArtifacts(product, inputs.staticlibrary, 'cpp', 'staticLibraries')
            property var rpaths: ModUtils.appendAll(product, 'rpaths')
        }

        prepare: {
            var args = Gcc.configFlags(product);
            for (var i in inputs.obj)
                args.push(inputs.obj[i].fileName)
            if (product.module.sysroot)
                args.push('--sysroot=' + product.module.sysroot)
            args.push('-Wl,-rpath,\$ORIGIN');
            args.push('-o');
            args.push(output.fileName);

            var staticLibrariesI = [];
            for (i in inputs.staticlibrary) {
                staticLibrariesI.push(inputs.staticlibrary[i].fileName);
            }
            staticLibrariesI = staticLibrariesI.concat(staticLibraries);

            var dynamicLibrariesI = [];
            for (i in dynamicLibraries) {
                if (dynamicLibraries[i].match("lib.*\\.so$") != null) {
                    // shared object filename found
                    var libDir = FileInfo.path(dynamicLibraries[i])
                    var libName = FileInfo.fileName(dynamicLibraries[i])
                    libName = libName.substr(3, libName.length - 6)
                    libraryPaths.push(libDir)
                    dynamicLibrariesI.push(libName)
                } else {
                    // shared object libname found
                    dynamicLibrariesI.push(dynamicLibraries[i])
                }
            }

            if (product.modules.qbs.targetOS === 'linux') {
                var transitiveSOs = ModUtils.appendAllFromArtifacts(product, inputs.dynamiclibrary, 'cpp', 'transitiveSOs')
                for (i in transitiveSOs) {
                    args.push("-Wl,-rpath-link=" + FileInfo.path(transitiveSOs[i]))
                }
            }

            for (i in inputs.dynamiclibrary) {
                libraryPaths.push(FileInfo.path(inputs.dynamiclibrary[i].fileName))
                var fileName = FileInfo.fileName(inputs.dynamiclibrary[i].fileName)
                fileName = fileName.substr(3, fileName.length - 6)
                dynamicLibrariesI.push(fileName)
            }

            args = args.concat(Gcc.libs(libraryPaths, rpaths, dynamicLibrariesI, staticLibrariesI));
            var cmd = new Command(product.module.compilerPath, args);
            Gcc.linkEnv(cmd, product)
            cmd.description = 'linking ' + FileInfo.fileName(output.fileName);
            cmd.highlight = 'linker'
            return cmd;
        }
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c"]
        explicitlyDependsOn: ["c++_pch"]

        Artifact {
            fileTags: ["obj"]
            // ### make it possible to override ".obj" in a project file
            fileName: ".obj/" + product.name + "/" + input.baseDir + "/" + input.baseName + ".o"
        }

        TransformProperties {
            property var defines: ModUtils.appendAll(input, 'defines')
            property var includePaths: ModUtils.appendAll(input, 'includePaths')
            property var compilerFlags: ModUtils.appendAll(input, 'compilerFlags')
        }

        prepare: {
            var args = Gcc.configFlags(input);

            // ### what we actually need here is something like product.usedFileTags
            //     that contains all fileTags that have been used when applying the rules.
            if (product.type.indexOf('staticlibrary') >= 0 || product.type.indexOf('dynamiclibrary') >= 0) {
                if (product.modules.qbs.toolchain !== "mingw")
                    args.push('-fPIC');
            } else if (product.type.indexOf('application') < 0) {
                throw ("OK, now we got a problem... The product's type must be in {staticlibrary, dynamiclibrary, application}. But it is " + product.type + '.');
            }

            if (product.module.sysroot)
                args.push('--sysroot=' + product.module.sysroot)
            for (var i in defines)
                args.push('-D' + defines[i]);
            for (var i in includePaths)
                args.push('-I' + includePaths[i]);
            if (product.module.precompiledHeader) {
                args.push('-include')
                args.push(product.name)
                var pchPath = product.module.precompiledHeaderDir[0]
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
                args.push('-x')
                args.push('c')
            }
            args.push('-c');
            args.push(input.fileName);
            args.push('-o');
            args.push(output.fileName);
            args = args.concat(compilerFlags);
            var cmd = new Command(product.module.compilerPath, args);
            cmd.description = 'compiling ';
            Gcc.describe(cmd, input.fileName, output.fileName);
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
            for (var i in product.module.defines)
                args.push('-D' + defines[i]);
            for (var i in product.module.includePaths)
                args.push('-I' + includePaths[i]);
            args.push('-x');
            args.push('c++-header');
            args.push('-c');
            args.push(product.module.precompiledHeader);
            args.push('-o');
            args.push(output.fileName);
            if (product.module.compilerFlags)
                args = args.concat(product.module.compilerFlags);
            var cmd = new Command(product.module.compilerPath, args);
            cmd.description = 'precompiling ' + FileInfo.fileName(input.fileName);
            return cmd;
        }
    }
}

