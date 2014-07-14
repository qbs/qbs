import qbs 1.0
import qbs.File
import qbs.FileInfo
import qbs.ModUtils

Module {
    condition: qbs.targetOS.contains("windows")

    property path toolchainInstallPath: qbs.getNativeSetting(registryKey)

    property string version: (versionMajor !== undefined && versionMinor !== undefined) ? (versionMajor + "." + versionMinor) : undefined
    property var versionParts: [ versionMajor, versionMinor, versionPatch, versionBuild ]
    property int versionMajor: qbs.getNativeSetting(registryKey, "VersionMajor")
    property int versionMinor: qbs.getNativeSetting(registryKey, "VersionMinor")
    property int versionPatch: qbs.getNativeSetting(registryKey, "VersionPatch")
    property int versionBuild: qbs.getNativeSetting(registryKey, "VersionRevision")

    property string compilerName: "makensis"
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

    // Private properties
    property string registryKey: {
        if (!qbs.hostOS.contains("windows"))
            return undefined;

        var keys = [ "HKEY_LOCAL_MACHINE\\SOFTWARE\\NSIS", "HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\NSIS" ];
        for (var i in keys) {
            if (qbs.getNativeSetting(keys[i]))
                return keys[i];
        }
    }

    validate: {
        var validator = new ModUtils.PropertyValidator("nsis");

        // Only *require* the toolchain install path on Windows
        // On other (Unix-like) operating systems it'll probably be in the PATH
        if (qbs.targetOS.contains("windows"))
            validator.setRequiredProperty("toolchainInstallPath", toolchainInstallPath);

        validator.setRequiredProperty("versionMajor", versionMajor);
        validator.setRequiredProperty("versionMinor", versionMinor);
        validator.setRequiredProperty("versionPatch", versionPatch);
        validator.setRequiredProperty("versionBuild", versionBuild);
        validator.addVersionValidator("version", version, 2, 4);
        validator.addRangeValidator("versionMajor", versionMajor, 1);
        validator.addRangeValidator("versionMinor", versionMinor, 0);
        validator.addRangeValidator("versionPatch", versionPatch, 0);
        validator.addRangeValidator("versionBuild", versionBuild, 0);
        validator.validate();
    }

    setupBuildEnvironment: {
        if (toolchainInstallPath) {
            var v = new ModUtils.EnvironmentVariable("PATH", ";", true);
            v.prepend(toolchainInstallPath);
            v.prepend(FileInfo.joinPaths(toolchainInstallPath, "bin"));
            v.set();
        }
    }

    // NSIS Script File
    FileTagger {
        patterns: ["*.nsi"]
        fileTags: ["nsi"]
    }

    // NSIS Header File
    FileTagger {
        patterns: ["*.nsh"]
        fileTags: ["nsh"]
    }

    Rule {
        id: nsisCompiler
        multiplex: true
        inputs: ["nsi"]

        Artifact {
            fileTags: ["nsissetup", "application"]
            filePath: product.destinationDirectory + "/" + product.targetName + ModUtils.moduleProperty(product, "executableSuffix")
        }

        prepare: {
            var i;
            var args = [];

            // Prefix character for makensis options
            var opt = product.moduleProperty("qbs", "hostOS").contains("windows") ? "/" : "-";

            if (ModUtils.moduleProperty(product, "disableConfig")) {
                args.push(opt + "NOCONFIG");
            }

            var warningLevel = ModUtils.moduleProperty(product, "warningLevel");
            var warningLevels = ["none", "errors", "warnings", "info", "all"];
            if (warningLevel !== "normal") {
                var level = warningLevels.indexOf(warningLevel);
                if (level < 0) {
                    throw("Unexpected warning level '" + warningLevel + "'.");
                } else {
                    args.push(opt + "V" + level);
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
                            args.push(opt + "D" + prefix + prop + "=" + val);
                        }
                    }
                }

                // Users are likely to need this
                var arch = product.moduleProperty("qbs", "architecture");
                args.push(opt + "Dqbs.architecture=" + arch);

                // Helper define for alternating between 32-bit and 64-bit logic
                if (arch === "x86_64" || arch === "ia64") {
                    args.push(opt + "DWin64");
                }
            }

            // User-supplied defines
            var defines = ModUtils.moduleProperty(product, "defines");
            for (i in defines) {
                args.push(opt + "D" + defines[i]);
            }

            // User-supplied flags
            var flags = ModUtils.moduleProperty(product, "compilerFlags");
            for (i in flags) {
                args.push(flags[i]);
            }

            // Override the compression algorithm if needed
            var compressor = ModUtils.moduleProperty(product, "compressor");
            if (compressor !== "default") {
                var compressorFlag = opt + "XSetCompressor /FINAL ";
                if (compressor.endsWith("-solid")) {
                    compressorFlag += "/SOLID ";
                }
                args.push(compressorFlag + compressor.split('-')[0]);
            }

            var inputFileNames = [];
            for (i in inputs.nsi) {
                inputFileNames.push(inputs.nsi[i].fileName);
                if (product.moduleProperty("qbs", "hostOS").contains("windows")) {
                    args.push(FileInfo.toWindowsSeparators(inputs.nsi[i].filePath));
                } else {
                    args.push(inputs.nsi[i].filePath);
                }
            }

            // Output file name - this goes last to override any OutFile command in the script
            args.push(opt + "XOutFile " + output.filePath);

            var cmd = new Command(ModUtils.moduleProperty(product, "compilerPath"), args);
            cmd.description = "compiling " + inputFileNames.join(", ");
            cmd.highlight = "compiler";
            cmd.workingDirectory = FileInfo.path(output.filePath);
            return cmd;
        }
    }
}
