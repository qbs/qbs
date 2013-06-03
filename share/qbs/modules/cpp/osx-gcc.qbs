import qbs 1.0
import '../utils.js' as ModUtils
import 'darwin-tools.js' as Tools
import "gcc.js" as Gcc

DarwinGCC {
    condition: qbs.hostOS === 'osx' && qbs.targetOS === 'osx' && qbs.toolchain === 'gcc'

    defaultInfoPlist: {
        var baseName = String(product.targetName).substring(product.targetName.lastIndexOf('/') + 1);
        var baseNameRfc1034 = Tools.rfc1034(baseName);
        var defaultVal;
        if (product.type.indexOf("applicationbundle") !== -1) {
            defaultVal = {
                CFBundleName: baseName,
                CFBundleIdentifier: "org.example." + baseNameRfc1034,
                CFBundleInfoDictionaryVersion: "6.0",
                CFBundleVersion: "1.0", // version of the app
                CFBundleShortVersionString: "1.0", // user visible version of the app
                CFBundleExecutable: baseName,
                CFBundleDisplayName: baseName,
                CFBundleIconFile: baseName + ".icns",
                CFBundlePackageType: "APPL",
                CFBundleSignature: "????", // legacy creator code in Mac OS Classic, can be ignored
                CFBundleDevelopmentRegion: "en" // default localization
            }
        } else if (product.type.indexOf("frameworkbundle") !== -1) {
            defaultVal = {
                CFBundleDisplayName: product.targetName,
                CFBundleName: product.targetName,
                CFBundleVersion: product.version || "1.0.0",
                CFBundlePackageType: "FMWK",
                CFBundleSignature: "????"
            };
        }
        return defaultVal
    }

    Rule {
        multiplex: true
        inputs: ["dynamiclibrary", "infoplist", "dsym"]

        Artifact {
            fileName: product.targetName + ".framework"
            fileTags: ["frameworkbundle"]
        }

        prepare: {
            var commands = [];
            var cmd = new Command("ln", ["-s", Gcc.majorVersion(product.version, "1"), "Current"]);
            cmd.workingDirectory = output.fileName + "/Versions";
            cmd.description = "creating framework " + product.targetName;
            cmd.highlight = "codegen";
            commands.push(cmd);

            cmd = new Command("ln", ["-s", "Versions/Current/Resources", "Resources"]);
            cmd.workingDirectory = output.fileName;
            commands.push(cmd);

            cmd = new Command("ln", ["-s", "Versions/Current/" + product.targetName, product.targetName]);
            cmd.workingDirectory = output.fileName;
            commands.push(cmd);
            return commands;
        }
    }
}
