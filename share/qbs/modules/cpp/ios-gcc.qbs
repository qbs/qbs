import qbs 1.0
import qbs.File
import qbs.FileInfo
import '../utils.js' as ModUtils
import 'darwin-tools.js' as DarwinTools
import 'bundle-tools.js' as BundleTools

DarwinGCC {
    condition: qbs.hostOS.contains('osx') && qbs.targetOS.contains('ios') && qbs.toolchain.contains('gcc')

    property string signingIdentity
    property string provisionFile
    property bool buildIpa: !qbs.targetOS.contains('ios-simulator')
    visibility: "hidden"
    optimization: {
        if (qbs.buildVariant === "debug")
            return "none";
        return qbs.targetOS.contains('ios-simulator') ? "fast" : "small"
    }

    platformCommonCompilerFlags: base.concat(["-fvisibility-inlines-hidden", "-g", "-gdwarf-2", "-fPIE"])
    commonCompilerFlags: ["-fpascal-strings", "-fexceptions", "-fasm-blocks", "-fstrict-aliasing"]
    linkerFlags: base.concat(["-dead_strip", "-headerpad_max_install_names"])

    Rule {
        multiplex: true
        inputs: ["qbs"]

        Artifact {
            fileName: product.destinationDirectory + "/"
                    + BundleTools.contentsFolderPath(product)
                    + "/ResourceRules.plist"
            fileTags: ["resourcerules"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating ResourceRules";
            cmd.highlight = "codegen";
            cmd.sysroot = product.moduleProperty("qbs","sysroot");
            cmd.sourceCode = function() {
                File.copy(sysroot + "/ResourceRules.plist", outputs.resourcerules[0].fileName);
            }
            return cmd;
        }
    }

    Rule {
        condition: product.moduleProperty("cpp", "buildIpa")
        multiplex: true
        inputs: ["application", "infoplist", "pkginfo", "resourcerules", "nib"]

        Artifact {
            fileName: product.destinationDirectory + "/" + product.targetName + ".ipa"
            fileTags: ["ipa"]
        }

        prepare: {
            var signingIdentity = product.moduleProperty("cpp", "signingIdentity");
            if (!signingIdentity)
                throw "The name of a valid Signing identity should be stored in cpp.signingIdentity  to build package.";
            var provisionFile = product.moduleProperty("cpp", "provisionFile");
            if (!provisionFile)
                throw "The path to a provision file is required in key cpp.provisionFile to build package.";
            args = ["-sdk", "iphoneos", "PackageApplication", "-v",
                    product.buildDirectory + "/" + product.targetName + ".app",
                    "-o", outputs.ipa[0].fileName, "--sign", signingIdentity,
                    "--embed", provisionFile]
            var command = "/usr/bin/xcrun";
            var cmd = new Command(command, args)
            cmd.description = "creating ipa";
            cmd.highlight = "codegen";
            cmd.workingDirectory = product.buildDirectory;
            return cmd;
        }
    }
}
