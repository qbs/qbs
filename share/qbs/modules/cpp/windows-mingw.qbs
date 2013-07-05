import qbs 1.0
import qbs.FileInfo
import '../utils.js' as ModUtils
import "windows.js" as Windows

GenericGCC {
    condition: qbs.targetOS.contains("windows") && qbs.toolchain.contains("mingw")
    staticLibraryPrefix: "lib"
    dynamicLibraryPrefix: ""
    executablePrefix: ""
    staticLibrarySuffix: ".a"
    dynamicLibrarySuffix: ".dll"
    executableSuffix: ".exe"
    windowsApiCharacterSet: "unicode"
    platformDefines: base.concat(Windows.characterSetDefines(windowsApiCharacterSet))
    compilerDefines: ['__GNUC__', 'WIN32', '_WIN32']

    property string windresName: 'windres'
    property string windresPath: { return toolchainPathPrefix + windresName }

    setupBuildEnvironment: {
        var v = new ModUtils.EnvironmentVariable("PATH", ";", true);
        v.prepend(toolchainInstallPath);
        v.set();
    }

    setupRunEnvironment: {
        var v = new ModUtils.EnvironmentVariable("PATH", ";", true);
        v.prepend(toolchainInstallPath);
        v.set();
    }

    FileTagger {
        pattern: "*.rc"
        fileTags: ["rc"]
    }

    Rule {
        inputs: ["rc"]

        Artifact {
            fileName: ".obj/" + product.name + "/" + input.baseDir.replace(':', '') + "/" + input.completeBaseName + "_res.o"
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
                args.push('-D');
                args.push(platformDefines[i]);
            }
            for (i in defines) {
                args.push('-D');
                args.push(defines[i]);
            }
            for (i in includePaths) {
                args.push('-I');
                args.push(includePaths[i]);
            }
            for (i in systemIncludePaths) {
                args.push('-isystem');
                args.push(systemIncludePaths[i]);
            }

            args = args.concat(['-i', input.fileName, '-o', output.fileName]);
            var cmd = new Command(ModUtils.moduleProperty(product, "windresPath"), args);
            cmd.description = 'compiling ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'compiler';
            return cmd;
        }
    }
}

