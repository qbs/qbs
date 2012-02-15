import qbs.base 1.0
import qbs.fileinfo 1.0 as FileInfo
import '../utils.js' as ModUtils
import 'msvc.js' as MSVC

CppModule {
    condition: qbs.hostOS == 'windows' && qbs.targetOS == 'windows' && qbs.toolchain == 'msvc'

    id: module

    defines: ['UNICODE', 'WIN32']
    warningLevel: "default"

    property bool generateManifestFiles: true
    property string toolchainInstallPath: "UNKNOWN"
    property string windowsSDKPath: "UNKNOWN"
    property string architecture: "x86"
    property int responseFileThreshold: 32000

    setupBuildEnvironment: {
        var v = new ModUtils.EnvironmentVariable("INCLUDE", ";", true)
        v.prepend(toolchainInstallPath + "/VC/ATLMFC/INCLUDE")
        v.prepend(toolchainInstallPath + "/VC/INCLUDE")
        v.prepend(windowsSDKPath + "/include")
        v.set()

        if (architecture == 'x86') {
            v = new ModUtils.EnvironmentVariable("PATH", ";", true)
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

    Artifact {
        // This adds the filename in precompiledHeader to the set of source files.
        // If its already in there, then this makes sure it has the right file tag.
        condition: precompiledHeader != null
        fileName: precompiledHeader
        fileTags: ["hpp_pch"]
    }

    Rule {
        id: pchCompiler
        inputs: ["hpp_pch"]
        Artifact {
            fileTags: ['obj']
            // ### make the object file dir overridable
            fileName: ".obj/" + product.name + "/" + input.baseName + '.obj'
        }
        Artifact {
            fileTags: ['c++_pch']
            // ### make the object file dir overridable
            fileName: ".obj/" + product.name + "/" + product.name + '.pch'
        }
        TransformProperties {
            property var defines: ModUtils.appendAll(input, 'defines')
            property var includePaths: ModUtils.appendAll(input, 'includePaths')
            property var compilerFlags: ModUtils.appendAll(input, 'compilerFlags')
        }
        prepare: {
            return MSVC.prepareCompiler(product, input, outputs)
        }
    }

    Rule {
        id: compiler
        inputs: ["cpp", "c"]
        explicitlyDependsOn: ["c++_pch"]
        Artifact {
            fileTags: ['obj']
            // ### make the object file dir overridable
            fileName: ".obj/" + product.name + "/" + input.baseDir + "/" + input.baseName + ".obj"
        }
 
        TransformProperties {
            property var defines: ModUtils.appendAll(input, 'defines')
            property var includePaths: ModUtils.appendAll(input, 'includePaths')
            property var compilerFlags: ModUtils.appendAll(input, 'compilerFlags')
        }

        prepare: {
            return MSVC.prepareCompiler(product, input, outputs, defines, includePaths, compilerFlags)
        }
    }

    Rule {
        id: applicationLinker
        multiplex: true
        inputs: ['obj']
        usings: ['staticlibrary', 'dynamiclibrary_import']
        Artifact {
            fileTags: ["application"]
            fileName: product.destinationDirectory + "/" + product.name + ".exe"
        }

        TransformProperties {
            property var libraryPaths: ModUtils.appendAll(product, 'libraryPaths')
            property var dynamicLibraries: ModUtils.appendAllFromArtifacts(product, inputs.dynamiclibrary_import, 'cpp', 'dynamicLibraries')
            property var staticLibraries: ModUtils.appendAllFromArtifacts(product, (inputs.staticlibrary || []).concat(inputs.obj), 'cpp', 'staticLibraries')
        }

        prepare: {
            return MSVC.prepareLinker(product, inputs, outputs, libraryPaths, dynamicLibraries, staticLibraries)
        }
    }

    Rule {
        id: dynamicLibraryLinker
        multiplex: true
        inputs: ['obj']
        usings: ['staticlibrary', 'dynamiclibrary_import']

        Artifact {
            fileTags: ["dynamiclibrary"]
            fileName: product.destinationDirectory + "/" + product.name + ".dll"
        }

        Artifact {
            fileTags: ["dynamiclibrary_import"]
            fileName: product.destinationDirectory + "/" + product.name + "_imp.lib"
        }

        TransformProperties {
            property var libraryPaths: ModUtils.appendAll(product, 'libraryPaths')
            property var dynamicLibraries: ModUtils.appendAll(product, 'dynamicLibraries')
            property var staticLibraries: ModUtils.appendAllFromArtifacts(product, (inputs.staticlibrary || []).concat(inputs.obj), 'cpp', 'staticLibraries')
        }

        prepare: {
            return MSVC.prepareLinker(product, inputs, outputs, libraryPaths, dynamicLibraries, staticLibraries)
        }
    }

    Rule {
        id: libtool
        multiplex: true
        inputs: ["obj"]
        usings: ["staticlibrary"]

        Artifact {
            fileTags: ["staticlibrary"]
            fileName: product.destinationDirectory + "/" + product.name + ".lib"
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
        }

        prepare: {
            var toolchainInstallPath = product.module.toolchainInstallPath

            var args = ['/nologo']
            var nativeOutputFileName = FileInfo.toWindowsSeparators(output.fileName)
            args.push('/OUT:' + nativeOutputFileName)
            for (var i in inputs.obj) {
                var fileName = FileInfo.toWindowsSeparators(inputs.obj[i].fileName)
                args.push(fileName)
            }
            var is64bit = (product.module.architecture == "x86_64")
            var linkerPath = toolchainInstallPath + '/VC/bin/'
            if (is64bit)
                linkerPath += 'amd64/'
            linkerPath += 'lib.exe'
            var cmd = new Command(linkerPath, args)
            cmd.description = 'creating ' + FileInfo.fileName(output.fileName)
            cmd.highlight = 'linker';
            cmd.workingDirectory = FileInfo.path(output.fileName)
            cmd.responseFileThreshold = product.module.responseFileThreshold
            cmd.responseFileUsagePrefix = '@';
            return cmd;
         }
    }
}
