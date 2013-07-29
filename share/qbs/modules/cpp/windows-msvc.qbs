import qbs 1.0
import qbs.File
import qbs.FileInfo
import '../utils.js' as ModUtils
import 'windows.js' as Windows
import 'msvc.js' as MSVC
import "bundle-tools.js" as BundleTools     // needed for path-tools.js
import 'path-tools.js' as PathTools

CppModule {
    condition: qbs.hostOS.contains('windows') && qbs.targetOS.contains('windows') && qbs.toolchain.contains('msvc')

    id: module

    windowsApiCharacterSet: "unicode"
    platformDefines: base.concat(Windows.characterSetDefines(windowsApiCharacterSet))
    compilerDefines: ['_WIN32']
    warningLevel: "default"
    compilerName: "cl.exe"

    property bool generateManifestFiles: true
    property string toolchainInstallPath
    property string windowsSDKPath
    property string architecture: qbs.architecture || "x86"
    staticLibraryPrefix: ""
    dynamicLibraryPrefix: ""
    executablePrefix: ""
    staticLibrarySuffix: ".lib"
    dynamicLibrarySuffix: ".dll"
    executableSuffix: ".exe"
    property string dynamicLibraryImportSuffix: ".lib"

    setupBuildEnvironment: {
        var vcDir = toolchainInstallPath.replace(/[\\/]bin$/i, "");
        var vcRootDir = vcDir.replace(/[\\/]VC$/i, "");

        var v = new ModUtils.EnvironmentVariable("INCLUDE", ";", true)
        v.prepend(vcDir + "/ATLMFC/INCLUDE")
        v.prepend(vcDir + "/INCLUDE")
        v.prepend(windowsSDKPath + "/include")
        v.set()

        if (architecture == 'x86') {
            v = new ModUtils.EnvironmentVariable("PATH", ";", true)
            v.prepend(windowsSDKPath + "/bin")
            v.prepend(vcRootDir + "/Common7/IDE")
            v.prepend(vcDir + "/bin")
            v.prepend(vcRootDir + "/Common7/Tools")
            v.set()

            v = new ModUtils.EnvironmentVariable("LIB", ";", true)
            v.prepend(vcDir + "/ATLMFC/LIB")
            v.prepend(vcDir + "/LIB")
            v.prepend(windowsSDKPath + "/lib")
            v.set()
        } else if (architecture == 'x86_64') {
            v = new ModUtils.EnvironmentVariable("PATH", ";", true)
            v.prepend(windowsSDKPath + "/bin/x64")
            v.prepend(vcRootDir + "/Common7/IDE")
            v.prepend(vcDir + "/bin/amd64")
            v.prepend(vcRootDir + "/Common7/Tools")
            v.set()

            v = new ModUtils.EnvironmentVariable("LIB", ";", true)
            v.prepend(vcDir + "/ATLMFC/LIB/amd64")
            v.prepend(vcDir + "/LIB/amd64")
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
                return ".obj/" + product.name + "/" + completeBaseName + '.obj'
            }
        }
        Artifact {
            fileTags: ['c++_pch']
            fileName: ".obj/" + product.name + "/" + product.name + '.pch'
        }
        prepare: {
            var platformDefines = ModUtils.moduleProperties(input, 'platformDefines');
            var defines = ModUtils.moduleProperties(input, 'defines');
            var includePaths = ModUtils.moduleProperties(input, 'includePaths');
            var systemIncludePaths = ModUtils.moduleProperties(input, 'systemIncludePaths');
            var cFlags = ModUtils.moduleProperties(input, 'platformCFlags').concat(
                        ModUtils.moduleProperties(input, 'cFlags'));
            var cxxFlags = ModUtils.moduleProperties(input, 'platformCxxFlags').concat(
                        ModUtils.moduleProperties(input, 'cxxFlags'));
            return MSVC.prepareCompiler(product, input, outputs, platformDefines, defines, includePaths, systemIncludePaths, cFlags, cxxFlags)
        }
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c"]
        auxiliaryInputs: ["hpp"]
        explicitlyDependsOn: ["c++_pch"]

        Artifact {
            fileTags: ['obj']
            fileName: ".obj/" + product.name + "/" + input.baseDir.replace(':', '') + "/" + input.fileName + ".obj"
        }

        prepare: {
            var platformDefines = ModUtils.moduleProperties(input, 'platformDefines');
            var defines = ModUtils.moduleProperties(input, 'defines');
            var includePaths = ModUtils.moduleProperties(input, 'includePaths');
            var systemIncludePaths = ModUtils.moduleProperties(input, 'systemIncludePaths');
            var cFlags = ModUtils.moduleProperties(input, 'platformCFlags').concat(
                        ModUtils.moduleProperties(input, 'cFlags'));
            var cxxFlags = ModUtils.moduleProperties(input, 'platformCxxFlags').concat(
                        ModUtils.moduleProperties(input, 'cxxFlags'));
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
            fileName: product.destinationDirectory + "/" + PathTools.applicationFilePath()
        }

        prepare: {
            var libraryPaths = ModUtils.moduleProperties(product, 'libraryPaths');
            var dynamicLibraries = ModUtils.moduleProperties(product, "dynamicLibraries");
            var staticLibraries = ModUtils.modulePropertiesFromArtifacts(product, inputs.staticlibrary, 'cpp', 'staticLibraries');
            var linkerFlags = ModUtils.moduleProperties(product, 'platformLinkerFlags').concat(
                        ModUtils.moduleProperties(product, 'linkerFlags'));
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
            fileName: product.destinationDirectory + "/" + PathTools.dynamicLibraryFilePath()
        }

        Artifact {
            fileTags: ["dynamiclibrary_import"]
            fileName: product.destinationDirectory + "/" + PathTools.importLibraryFilePath()
            alwaysUpdated: false
        }

        prepare: {
            var libraryPaths = ModUtils.moduleProperties(product, 'libraryPaths');
            var dynamicLibraries = ModUtils.moduleProperties(product, 'dynamicLibraries');
            var staticLibraries = ModUtils.modulePropertiesFromArtifacts(product, inputs.staticlibrary, 'cpp', 'staticLibraries');
            var linkerFlags = ModUtils.moduleProperties(product, 'platformLinkerFlags').concat(
                        ModUtils.moduleProperties(product, 'linkerFlags'));
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
            fileName: product.destinationDirectory + "/" + PathTools.staticLibraryFilePath()
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
        }

        prepare: {
            var args = ['/nologo']
            var nativeOutputFileName = FileInfo.toWindowsSeparators(output.fileName)
            args.push('/OUT:' + nativeOutputFileName)
            for (var i in inputs.obj) {
                var fileName = FileInfo.toWindowsSeparators(inputs.obj[i].fileName)
                args.push(fileName)
            }
            var cmd = new Command("lib.exe", args);
            cmd.description = 'creating ' + FileInfo.fileName(output.fileName)
            cmd.highlight = 'linker';
            cmd.workingDirectory = FileInfo.path(output.fileName)
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
