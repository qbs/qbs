import qbs
import qbs.ModUtils

UnixGCC {
    condition: false

    compilerDefines: ["__GNUC__", "__APPLE__"]
    loadableModulePrefix: ""
    loadableModuleSuffix: ".bundle"
    dynamicLibrarySuffix: ".dylib"
    debugInfoSuffix: ".dSYM"

    validate: {
        if (qbs.sysroot) {
            var validator = new ModUtils.PropertyValidator("cpp");
            validator.setRequiredProperty("xcodeSdkName", xcodeSdkName);
            validator.setRequiredProperty("xcodeSdkVersion", xcodeSdkVersion);
            validator.validate();
        }
    }

    setupBuildEnvironment: {
        var v = new ModUtils.EnvironmentVariable("PATH", ":", false);
        if (platformPath) {
            v.prepend(platformPath + "/Developer/usr/bin");
            var platformPathL = platformPath.split("/");
            platformPathL.pop();
            platformPathL.pop();
            var devPath = platformPathL.join("/")
            v.prepend(devPath + "/usr/bin");
            v.set();
        }
        for (var key in buildEnv) {
            v = new ModUtils.EnvironmentVariable(key);
            v.value = buildEnv[key];
            v.set();
        }
    }

    property var defaultInfoPlist: {
        var dict = {};

        if (qbs.targetOS.contains("osx")) {
            dict["NSPrincipalClass"] = "NSApplication"; // needed for Retina display support

            if (minimumOsxVersion)
                dict["LSMinimumSystemVersion"] = minimumOsxVersion;
        }

        if (qbs.targetOS.contains("ios")) {
            dict["LSRequiresIPhoneOS"] = true;

            // architectures supported, to support iPhone 3G for example one has to add
            // armv6 to the list and also compile for it (with Xcode 4.4.1 or earlier)
            if (!qbs.targetOS.contains("ios-simulator"))
                dict["UIRequiredDeviceCapabilities"] = [ "armv7" ];

            var orientations = [
                "UIInterfaceOrientationPortrait",
                "UIInterfaceOrientationLandscapeLeft",
                "UIInterfaceOrientationLandscapeRight"
            ];

            dict["UISupportedInterfaceOrientations"] = orientations;
            orientations.splice(1, 0, "UIInterfaceOrientationPortraitUpsideDown");
            dict["UISupportedInterfaceOrientations~ipad"] = orientations;
        }

        return dict;
    }

    // private properties
    readonly property var buildEnv: {
        var env = {
            "EXECUTABLE_NAME": product.targetName,
            "LANG": "en_US.US-ASCII",
            "PRODUCT_NAME": product.name
        }
        if (qbs.targetOS.contains("ios") && minimumIosVersion)
            env["IPHONEOS_DEPLOYMENT_TARGET"] = minimumIosVersion;
        if (qbs.targetOS.contains("osx") && minimumOsxVersion)
            env["MACOSX_DEPLOYMENT_TARGET"] = minimumOsxVersion;
        return env;
    }

    readonly property path platformInfoPlist: platformPath ? [platformPath, "Info.plist"].join("/") : undefined
    readonly property path sdkSettingsPlist: sysroot ? [sysroot, "SDKSettings.plist"].join("/") : undefined
    readonly property path toolchainInfoPlist: toolchainInstallPath ? [toolchainInstallPath, "../../ToolchainInfo.plist"].join("/") : undefined
}
