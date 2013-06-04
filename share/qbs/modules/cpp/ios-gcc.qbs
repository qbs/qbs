import qbs 1.0
import qbs.fileinfo as FileInfo
import '../utils.js' as ModUtils
import 'darwin-tools.js' as DarwinTools
import 'bundle-tools.js' as BundleTools

DarwinGCC {
    condition: qbs.hostOS === 'osx' && qbs.targetOS === 'ios' && qbs.toolchain === 'gcc'

    property string signingIdentity
    property string provisionFile
    property bool buildIpa: qbs.architecture.match("^arm") === "arm"

    defaultInfoPlist: {
        var baseName = String(product.targetName).substring(product.targetName.lastIndexOf('/') + 1);
        var baseNameRfc1034 = DarwinTools.rfc1034(baseName);
        var defaultVal = {
            CFBundleName: baseName,
            CFBundleIdentifier: "org.example." + baseNameRfc1034,
            CFBundleInfoDictionaryVersion : "6.0",
            CFBundleVersion: "1.0", // version of the app
            CFBundleShortVersionString: "1.0", // user visible version of the app
            // this and the following MainStoryboard related keys are iOS 6 only, and are only to
            // use "storyboards" for UI, which we most likely won't, so we can avoid using
            // these keys (and stay compatible with previous iOS versions)
            // "UIMainStoryboardFile" : "MainStoryboard_iPhone",
            // "UIMainStoryboardFile~ipad" : "MainStoryboard_iPad",
            CFBundleExecutable: baseName,
            LSRequiresIPhoneOS: true,
            UIRequiredDeviceCapabilities : [
                // architectures supported, to support iPhone 3G for example one has to add
                // armv6 to the list and also compile for it (with Xcode 4.4.1 or earlier)
                "armv7"
            ],
            CFBundleDisplayName: baseName,
            CFBundlePackageType: "APPL",
            CFBundleSignature: "????", // legacy creator code in Mac OS Classic, can be ignored
            CFBundleDevelopmentRegion: "en", // default localization
            UISupportedInterfaceOrientations: [
                "UIInterfaceOrientationPortrait",
                "UIInterfaceOrientationLandscapeLeft",
                "UIInterfaceOrientationLandscapeRight"
            ],
            "UISupportedInterfaceOrientations~ipad": [
                "UIInterfaceOrientationPortrait",
                "UIInterfaceOrientationPortraitUpsideDown",
                "UIInterfaceOrientationLandscapeLeft",
                "UIInterfaceOrientationLandscapeRight"
            ]
        }
        return defaultVal
    }

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
        multiplex: true
        inputs: ["application", "infoplist", "pkginfo", "resourcerules", "nib"]

        Artifact {
            fileName: product.destinationDirectory + "/" + product.targetName + ".ipa"
            fileTags: ["ipa"]
        }

        prepare: {
            if (!(product.moduleProperty("qbs","architecture").match("^arm"))) {
                // to be removed when dynamic dependencies work
                var cmd = new JavaScriptCommand();
                cmd.description = "skipping ipa for simulator on arch " +
                        product.moduleProperty("qbs", "architecture");
                cmd.highlight = "codegen";
                return cmd;
            }
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
