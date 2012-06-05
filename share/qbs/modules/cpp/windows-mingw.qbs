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

        TransformProperties {
            property var platformDefines: ModUtils.appendAll(input, 'platformDefines')
            property var defines: ModUtils.appendAll(input, 'defines')
            property var includePaths: ModUtils.appendAll(input, 'includePaths')
        }

        prepare: {
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

            args = args.concat(['-i', input.fileName, '-o', output.fileName]);
            var cmd = new Command('windres', args);
            cmd.description = 'compiling ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'compiler';
            return cmd;
        }
    }
}

