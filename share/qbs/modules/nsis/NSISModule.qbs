import qbs 1.0
import qbs.File
import qbs.FileInfo
import "../utils.js" as ModUtils

Module {
    condition: qbs.hostOS.contains("windows") && qbs.targetOS.contains("windows")

    property path toolchainInstallPath: {
        // First try the registry key...
        // HKLM\\SOFTWARE\\Wow6432Node\\NSIS\\(default),VersionMajor,VersionMinor,VersionBuild,VersionRevision
        // HKLM\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\NSIS\\DisplayVersion

        // Then try the known default filesystem path
        var path = FileInfo.joinPaths(qbs.getenv("PROGRAMFILES(X86)"), "NSIS");
        if (File.exists(path))
            return path;
    }

    property string compilerName: "makensis.exe"
    property string compilerPath: compilerName

    property string warningLevel: "normal"
    PropertyOptions {
        name: "warningLevel"
        allowedValues: ["none", "normal", "errors", "warnings", "info", "all"]
    }

    property bool disableConfig: false
    PropertyOptions {
        name: "disableConfig"
        description: "disable inclusion of nsisconf.nsh"
    }

    property bool enableQbsDefines: true
    PropertyOptions {
        name: "enableQbsDefines"
        description: "built-in variables that are defined when using the NSIS compiler"
    }

    property stringList defines
    PropertyOptions {
        name: "defines"
        description: "variables that are defined when using the NSIS compiler"
    }

    property stringList compilerFlags
    PropertyOptions {
        name: "compilerFlags"
        description: "additional flags for the NSIS compiler"
    }

    property string compressor: "default"
    PropertyOptions {
        name: "compressor"
        description: "the compression algorithm used to compress files/data in the installer"
        allowedValues: ["default", "zlib", "zlib-solid", "bzip2", "bzip2-solid", "lzma", "lzma-solid"]
    }

    property string executableSuffix: ".exe"

    validate: {
        if (!toolchainInstallPath)
            throw "nsis.toolchainInstallPath is not defined. Set nsis.toolchainInstallPath in your profile.";
    }

    setupBuildEnvironment: {
        var v = new ModUtils.EnvironmentVariable("PATH", ";", true);
        v.prepend(toolchainInstallPath);
        v.prepend(FileInfo.joinPaths(toolchainInstallPath, "bin"));
        v.set();
    }

    // NSIS Script File
    FileTagger {
        pattern: "*.nsi"
        fileTags: ["nsi"]
    }

    // NSIS Header File
    FileTagger {
        pattern: "*.nsh"
        fileTags: ["nsh"]
    }

    Rule {
        id: nsisCompiler
        multiplex: true
        inputs: ["nsi"]

        Artifact {
            fileTags: ["nsissetup", "application"]
            fileName: product.destinationDirectory + "/" + product.targetName + ModUtils.moduleProperty(product, "executableSuffix")
        }

        prepare: {
            var i;
            var args = [];

            if (ModUtils.moduleProperty(product, "disableConfig")) {
                args.push("/NOCONFIG");
            }

            var warningLevel = ModUtils.moduleProperty(product, "warningLevel");
            var warningLevels = ["none", "errors", "warnings", "info", "all"];
            if (warningLevel !== "normal") {
                var level = warningLevels.indexOf(warningLevel);
                if (level < 0) {
                    throw("Unexpected warning level '" + warningLevel + "'.");
                } else {
                    args.push("/V" + level);
                }
            }

            var enableQbsDefines = ModUtils.moduleProperty(product, "enableQbsDefines")
            if (enableQbsDefines) {
                var map = {
                    "project.": project,
                    "product.": product
                };

                for (var prefix in map) {
                    var obj = map[prefix];
                    for (var prop in obj) {
                        var val = obj[prop];
                        if (typeof val !== 'function' && typeof val !== 'object' && !prop.startsWith("_")) {
                            args.push("/D" + prefix + prop + "=" + val);
                        }
                    }
                }

                // Users are likely to need this
                var arch = product.moduleProperty("qbs", "architecture");
                args.push("/Dqbs.architecture=" + arch);

                // Helper define for alternating between 32-bit and 64-bit logic
                if (arch === "x86_64" || arch === "ia64") {
                    args.push("/DWin64");
                }
            }

            // User-supplied defines
            var defines = ModUtils.moduleProperty(product, "defines");
            for (i in defines) {
                args.push("/D" + defines[i]);
            }

            // User-supplied flags
            var flags = ModUtils.moduleProperty(product, "compilerFlags");
            for (i in flags) {
                args.push(flags[i]);
            }

            var inputFileNames = [];
            for (i in inputs.nsi) {
                inputFileNames.push(FileInfo.fileName(inputs.nsi[i].fileName));
                args.push(FileInfo.toWindowsSeparators(inputs.nsi[i].fileName));
            }

            // These flags go last so they override any commands in the NSIS script...

            // Output file name
            args.push("/XOutFile " + output.fileName);

            // Set the compression algorithm
            var compressor = ModUtils.moduleProperty(product, "compressor");
            if (compressor !== "default") {
                var compressorFlag = "/XSetCompressor ";
                if (compressor.endsWith("-solid")) {
                    compressorFlag += "/SOLID ";
                }
                args.push(compressorFlag + "/FINAL " + compressor);
            }

            var cmd = new Command(ModUtils.moduleProperty(product, "compilerPath"), args);
            cmd.description = "compiling " + inputFileNames.join(", ");
            cmd.highlight = "compiler";
            cmd.workingDirectory = FileInfo.path(output.fileName);
            return cmd;
        }
    }
}
