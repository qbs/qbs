import qbs 1.0
import qbs.File
import qbs.FileInfo
import qbs.ModUtils
import qbs.PathTools
import qbs.WindowsUtils
import 'msvc.js' as MSVC

CppModule {
    condition: qbs.hostOS.contains('windows') && qbs.targetOS.contains('windows') && qbs.toolchain.contains('msvc')

    id: module

    windowsApiCharacterSet: "unicode"
    platformDefines: base.concat(WindowsUtils.characterSetDefines(windowsApiCharacterSet))
    compilerDefines: ['_WIN32']
    warningLevel: "default"
    compilerName: "cl.exe"
    linkerName: "link.exe"
    runtimeLibrary: "dynamic"

    property bool generateManifestFiles: true
    property path toolchainInstallPath
    architecture: qbs.architecture
    staticLibraryPrefix: ""
    dynamicLibraryPrefix: ""
    executablePrefix: ""
    staticLibrarySuffix: ".lib"
    dynamicLibrarySuffix: ".dll"
    executableSuffix: ".exe"
    property string dynamicLibraryImportSuffix: ".lib"

    Transformer {
        condition: cPrecompiledHeader !== undefined
        inputs: cPrecompiledHeader
        Artifact {
            fileTags: ['obj']
            filePath: {
                var completeBaseName = FileInfo.completeBaseName(product.moduleProperty("cpp",
                        "cPrecompiledHeader"));
                return ".obj/" + completeBaseName + '_c.obj'
            }
        }
        Artifact {
            fileTags: ['c_pch']
            filePath: ".obj/" + product.name + '_c.pch'
        }
        prepare: {
            return MSVC.prepareCompiler(product, input, outputs);
        }
    }

    Transformer {
        condition: cxxPrecompiledHeader !== undefined
        inputs: cxxPrecompiledHeader
        explicitlyDependsOn: ["c_pch"]  // to prevent vc--0.pdb conflict
        Artifact {
            fileTags: ['obj']
            filePath: {
                var completeBaseName = FileInfo.completeBaseName(product.moduleProperty("cpp",
                        "cxxPrecompiledHeader"));
                return ".obj/" + completeBaseName + '_cpp.obj'
            }
        }
        Artifact {
            fileTags: ['cpp_pch']
            filePath: ".obj/" + product.name + '_cpp.pch'
        }
        prepare: {
            return MSVC.prepareCompiler(product, input, outputs);
        }
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c"]
        auxiliaryInputs: ["hpp"]
        explicitlyDependsOn: ["c_pch", "cpp_pch"]

        Artifact {
            fileTags: ['obj']
            filePath: ".obj/" + input.baseDir.replace(':', '') + "/" + input.fileName + ".obj"
        }

        prepare: {
            return MSVC.prepareCompiler(product, input, outputs);
        }
    }

    Rule {
        id: applicationLinker
        multiplex: true
        inputs: ['obj']
        usings: ['staticlibrary', 'dynamiclibrary_import']
        Artifact {
            fileTags: ["application"]
            filePath: product.destinationDirectory + "/" + PathTools.applicationFilePath(product)
        }

        prepare: {
            return MSVC.prepareLinker(product, inputs, outputs);
        }
    }

    Rule {
        id: dynamicLibraryLinker
        multiplex: true
        inputs: ['obj']
        usings: ['staticlibrary', 'dynamiclibrary_import']

        Artifact {
            fileTags: ["dynamiclibrary"]
            filePath: product.destinationDirectory + "/" + PathTools.dynamicLibraryFilePath(product)
        }

        Artifact {
            fileTags: ["dynamiclibrary_import"]
            filePath: product.destinationDirectory + "/" + PathTools.importLibraryFilePath(product)
            alwaysUpdated: false
        }

        prepare: {
            return MSVC.prepareLinker(product, inputs, outputs);
        }
    }

    Rule {
        id: libtool
        multiplex: true
        inputs: ["obj"]
        usings: ["staticlibrary"]

        Artifact {
            fileTags: ["staticlibrary"]
            filePath: product.destinationDirectory + "/" + PathTools.staticLibraryFilePath(product)
            cpp.staticLibraries: {
                var result = []
                for (var i in inputs.staticlibrary) {
                    var lib = inputs.staticlibrary[i]
                    result.push(lib.filePath)
                    var impliedLibs = ModUtils.moduleProperties(lib, 'staticLibraries')
                    result = result.uniqueConcat(impliedLibs);
                }
                return result
            }
        }

        prepare: {
            var args = ['/nologo']
            var nativeOutputFileName = FileInfo.toWindowsSeparators(output.filePath)
            args.push('/OUT:' + nativeOutputFileName)
            for (var i in inputs.obj) {
                var fileName = FileInfo.toWindowsSeparators(inputs.obj[i].filePath)
                args.push(fileName)
            }
            var cmd = new Command("lib.exe", args);
            cmd.description = 'creating ' + output.fileName;
            cmd.highlight = 'linker';
            cmd.workingDirectory = FileInfo.path(output.filePath)
            cmd.responseFileUsagePrefix = '@';
            return cmd;
         }
    }

    FileTagger {
        patterns: ["*.rc"]
        fileTags: ["rc"]
    }

    Rule {
        inputs: ["rc"]

        Artifact {
            filePath: ".obj/" + input.baseDir.replace(':', '') + "/" + input.completeBaseName + ".res"
            fileTags: ["obj"]
        }

        prepare: {
            var platformDefines = ModUtils.moduleProperty(input, 'platformDefines');
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

            args = args.concat(['/fo', output.filePath, input.filePath]);
            var cmd = new Command('rc', args);
            cmd.description = 'compiling ' + input.fileName;
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
