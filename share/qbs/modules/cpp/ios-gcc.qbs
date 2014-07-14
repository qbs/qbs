import qbs 1.0
import qbs.BundleTools
import qbs.DarwinTools
import qbs.File
import qbs.ModUtils

DarwinGCC {
    condition: qbs.hostOS.contains('osx') && qbs.targetOS.contains('ios') && qbs.toolchain.contains('gcc')

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
        condition: !product.moduleProperty("qbs", "targetOS").contains("ios-simulator")
        multiplex: true
        inputs: ["qbs"]

        Artifact {
            filePath: product.destinationDirectory + "/"
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
                File.copy(sysroot + "/ResourceRules.plist", outputs.resourcerules[0].filePath);
            }
            return cmd;
        }
    }

    Rule {
        condition: product.moduleProperty("cpp", "buildIpa")
        multiplex: true
        inputs: ["application", "infoplist", "pkginfo", "resourcerules", "compiled_nib"]

        Artifact {
            filePath: product.destinationDirectory + "/" + product.targetName + ".ipa"
            fileTags: ["ipa"]
        }

        prepare: {
            var signingIdentity = product.moduleProperty("cpp", "signingIdentity");
            if (!signingIdentity)
                throw "The name of a valid signing identity must be set using " +
                        "cpp.signingIdentity in order to build an IPA package.";

            var provisioningProfile = product.moduleProperty("cpp", "provisioningProfile");
            if (!provisioningProfile)
                throw "The path to a provisioning profile must be set using " +
                        "cpp.provisioningProfile in order to build an IPA package.";

            var args = ["-sdk", product.moduleProperty("cpp", "xcodeSdkName"), "PackageApplication",
                        "-v", product.buildDirectory + "/" + BundleTools.wrapperName(product),
                        "-o", outputs.ipa[0].filePath, "--sign", signingIdentity,
                        "--embed", provisioningProfile];

            var command = "/usr/bin/xcrun";
            var cmd = new Command(command, args)
            cmd.description = "creating ipa, signing with " + signingIdentity;
            cmd.highlight = "codegen";
            cmd.workingDirectory = product.buildDirectory;
            return cmd;
        }
    }
}
