import qbs.base 1.0
import qbs.fileinfo 1.0 as FileInfo
import '../utils.js' as ModUtils

GenericGCC {
    condition: qbs.targetOS === "windows" && qbs.toolchain === "mingw"
    staticLibraryPrefix: "lib"
    dynamicLibraryPrefix: "lib"
    executablePrefix: ""
    staticLibrarySuffix: ".a"
    dynamicLibrarySuffix: ".dll"
    executableSuffix: ".exe"
    platformDefines: ['UNICODE']
    compilerDefines: ['__GNUC__', 'WIN32']

    setupBuildEnvironment: {
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
            fileName: ".obj/" + product.name + "/" + input.baseDir + "/" + input.baseName + "_res.o"
            fileTags: ["obj"]
        }

        prepare: {
            var cmd = new Command('windres', ['-i', input.fileName, '-o', output.fileName]);
            cmd.description = 'compiling ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'compiler';
            return cmd;
        }
    }
}

