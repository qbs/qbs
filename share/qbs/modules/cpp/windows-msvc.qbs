import qbs 1.0
import qbs.fileinfo as FileInfo
import '../utils.js' as ModUtils
import 'msvc.js' as MSVC

CppModule {
    condition: qbs.hostOS === 'windows' && qbs.targetOS === 'windows' && qbs.toolchain === 'msvc'

    id: module

    platformDefines: base.concat(['UNICODE'])
    compilerDefines: ['_WIN32']
    warningLevel: "default"

    property bool generateManifestFiles: true
    property string toolchainInstallPath: "UNKNOWN"
    property string windowsSDKPath: "UNKNOWN"
    property string architecture: qbs.architecture || "x86"
    property int responseFileThreshold: 32000
    staticLibraryPrefix: ""
    dynamicLibraryPrefix: ""
    executablePrefix: ""
    staticLibrarySuffix: ".lib"
    dynamicLibrarySuffix: ".dll"
    executableSuffix: ".exe"
    property string dynamicLibraryImportSuffix: ".lib"

    setupBuildEnvironment: {
        var v = new ModUtils.EnvironmentVariable("INCLUDE", ";", true)
        v.prepend(toolchainInstallPath + "/VC/ATLMFC/INCLUDE")
        v.prepend(toolchainInstallPath + "/VC/INCLUDE")
        v.prepend(windowsSDKPath + "/include")
        v.set()

        if (architecture == 'x86') {
            v = new ModUtils.EnvironmentVariable("PATH", ";", true)
            v.prepend(windowsSDKPath + "/bin")
            v.prepend(toolchainInstallPath + "/Common7/IDE")
            v.prepend(toolchainInstallPath + "/VC/bin")
            v.prepend(toolchainInstallPath + "/Common7/Tools")
            v.set()

            v = new ModUtils.EnvironmentVariable("LIB", ";", true)
            v.prepend(toolchainInstallPath + "/VC/ATLMFC/LIB")
            v.prepend(toolchainInstallPath + "/VC/LIB")
            v.prepend(windowsSDKPath + "/lib")
            v.set()
        } else if (architecture == 'x86_64') {
            v = new ModUtils.EnvironmentVariable("PATH", ";", true)
            v.prepend(windowsSDKPath + "/bin/x64")
            v.prepend(toolchainInstallPath + "/Common7/IDE")
            v.prepend(toolchainInstallPath + "/VC/bin/amd64")
            v.prepend(toolchainInstallPath + "/Common7/Tools")
            v.set()

            v = new ModUtils.EnvironmentVariable("LIB", ";", true)
            v.prepend(toolchainInstallPath + "/VC/ATLMFC/LIB/amd64")
            v.prepend(toolchainInstallPath + "/VC/LIB/amd64")
            v.prepend(windowsSDKPath + "/lib/x64")
            v.set()
        }
    }

    Transformer {
        condition: precompiledHeader !== undefined
        inputs: precompiledHeader
        Artifact {
            fileTags: ['obj']
            fileName: {
                var completeBaseName = FileInfo.completeBaseName(product.moduleProperty("cpp",
                        "precompiledHeader"));
                // ### make the object file dir overridable
                return ".obj/" + product.name + "/" + completeBaseName + '.obj'
            }
        }
        Artifact {
            fileTags: ['c++_pch']
            // ### make the object file dir overridable
            fileName: ".obj/" + product.name + "/" + product.name + '.pch'
        }
        prepare: {
            var platformDefines = ModUtils.moduleProperties(input, 'platformDefines');
            var defines = ModUtils.moduleProperties(input, 'defines');
            var includePaths = ModUtils.moduleProperties(input, 'includePaths');
            var systemIncludePaths = ModUtils.moduleProperties(input, 'systemIncludePaths');
            var cFlags = ModUtils.moduleProperties(input, 'cFlags');
            var cxxFlags = ModUtils.moduleProperties(input, 'cxxFlags');
            return MSVC.prepareCompiler(product, input, outputs, platformDefines, defines, includePaths, systemIncludePaths, cFlags, cxxFlags)
        }
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c"]
        explicitlyDependsOn: ["c++_pch"]
        Artifact {
            fileTags: ['obj']
            // ### make the object file dir overridable
            fileName: ".obj/" + product.name + "/" + input.baseDir.replace(':', '') + "/" + input.fileName + ".obj"
        }

        prepare: {
            var platformDefines = ModUtils.moduleProperties(input, 'platformDefines');
            var defines = ModUtils.moduleProperties(input, 'defines');
            var includePaths = ModUtils.moduleProperties(input, 'includePaths');
            var systemIncludePaths = ModUtils.moduleProperties(input, 'systemIncludePaths');
            var cFlags = ModUtils.moduleProperties(input, 'cFlags');
            var cxxFlags = ModUtils.moduleProperties(input, 'cxxFlags');
            return MSVC.prepareCompiler(product, input, outputs, platformDefines, defines, includePaths, systemIncludePaths, cFlags, cxxFlags)
        }
    }

    Rule {
        id: applicationLinker
        multiplex: true
        inputs: ['obj']
        usings: ['staticlibrary', 'dynamiclibrary_import']
        Artifact {
            fileTags: ["application"]
            fileName: product.destinationDirectory + "/" + ModUtils.moduleProperty(product, "executablePrefix")
                      + product.targetName + ModUtils.moduleProperty(product, "executableSuffix")
        }

        prepare: {
            var libraryPaths = ModUtils.moduleProperties(product, 'libraryPaths');
            var dynamicLibraries = ModUtils.modulePropertiesFromArtifacts(product, inputs.dynamiclibrary_import, 'cpp', 'dynamicLibraries');
            var staticLibraries = ModUtils.modulePropertiesFromArtifacts(product, (inputs.staticlibrary || []).concat(inputs.obj), 'cpp', 'staticLibraries');
            var linkerFlags = ModUtils.moduleProperties(product, 'linkerFlags');
            return MSVC.prepareLinker(product, inputs, outputs, libraryPaths, dynamicLibraries, staticLibraries, linkerFlags)
        }
    }

    Rule {
        id: dynamicLibraryLinker
        multiplex: true
        inputs: ['obj']
        usings: ['staticlibrary', 'dynamiclibrary_import']

        Artifact {
            fileTags: ["dynamiclibrary"]
            fileName: product.destinationDirectory + "/"
                      + ModUtils.moduleProperty(product, "dynamicLibraryPrefix") + product.targetName
                      + ModUtils.moduleProperty(product, "dynamicLibrarySuffix")
        }

        Artifact {
            fileTags: ["dynamiclibrary_import"]
            fileName: product.destinationDirectory + "/"
                      + ModUtils.moduleProperty(product, "dynamicLibraryPrefix") + product.targetName
                      + ModUtils.moduleProperty(product, "dynamicLibraryImportSuffix")
            alwaysUpdated: false
        }

        prepare: {
            var libraryPaths = ModUtils.moduleProperties(product, 'libraryPaths');
            var dynamicLibraries = ModUtils.moduleProperties(product, 'dynamicLibraries');
            var staticLibraries = ModUtils.modulePropertiesFromArtifacts(product, (inputs.staticlibrary || []).concat(inputs.obj), 'cpp', 'staticLibraries');
            var linkerFlags = ModUtils.moduleProperties(product, 'linkerFlags');
            return MSVC.prepareLinker(product, inputs, outputs, libraryPaths, dynamicLibraries, staticLibraries, linkerFlags)
        }
    }

    Rule {
        id: libtool
        multiplex: true
        inputs: ["obj"]
        usings: ["staticlibrary"]

        Artifact {
            fileTags: ["staticlibrary"]
            fileName: product.destinationDirectory + "/" + ModUtils.moduleProperty(product, "staticLibraryPrefix")
                      + product.targetName + ModUtils.moduleProperty(product, "staticLibrarySuffix")
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
        }

        prepare: {
            var toolchainInstallPath = ModUtils.moduleProperty(product, "toolchainInstallPath")

            var args = ['/nologo']
            var nativeOutputFileName = FileInfo.toWindowsSeparators(output.fileName)
            args.push('/OUT:' + nativeOutputFileName)
            for (var i in inputs.obj) {
                var fileName = FileInfo.toWindowsSeparators(inputs.obj[i].fileName)
                args.push(fileName)
            }
            var is64bit = (ModUtils.moduleProperty(product, "architecture") == "x86_64")
            var linkerPath = toolchainInstallPath + '/VC/bin/'
            if (is64bit)
                linkerPath += 'amd64/'
            linkerPath += 'lib.exe'
            var cmd = new Command(linkerPath, args)
            cmd.description = 'creating ' + FileInfo.fileName(output.fileName)
            cmd.highlight = 'linker';
            cmd.workingDirectory = FileInfo.path(output.fileName)
            cmd.responseFileThreshold = ModUtils.moduleProperty(product, "responseFileThreshold")
            cmd.responseFileUsagePrefix = '@';
            return cmd;
         }
    }

    FileTagger {
        pattern: "*.rc"
        fileTags: ["rc"]
    }

    Rule {
        inputs: ["rc"]

        Artifact {
            fileName: ".obj/" + product.name + "/" + input.baseDir.replace(':', '') + "/" + input.completeBaseName + ".res"
            fileTags: ["obj"]
        }

        prepare: {
            var platformDefines = ModUtils.moduleProperties(input, 'platformDefines');
            var defines = ModUtils.moduleProperties(input, 'defines');
            var includePaths = ModUtils.moduleProperties(input, 'includePaths');
            var systemIncludePaths = ModUtils.moduleProperties(input, 'systemIncludePaths');
            var args = [];
            var i;
            for (i in platformDefines) {
                args.push('/d');
                args.push(platformDefines[i]);
            }
            for (i in defines) {
                args.push('/d');
                args.push(defines[i]);
            }
            for (i in includePaths) {
                args.push('/i');
                args.push(includePaths[i]);
            }
            for (i in systemIncludePaths) {
                args.push('/i');
                args.push(systemIncludePaths[i]);
            }

            args = args.concat(['/fo', output.fileName, input.fileName]);
            var cmd = new Command('rc', args);
            cmd.description = 'compiling ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'compiler';

            // Remove the first two lines of stdout. That's the logo.
            // Unfortunately there's no command line switch to turn that off.
            cmd.stdoutFilterFunction = function(output) {
                var idx = 0;
                for (var i = 0; i < 3; ++i)
                    idx = output.indexOf('\n', idx + 1);
                return output.substr(idx + 1);
            }

            return cmd;
        }
    }
}
