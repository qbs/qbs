import qbs.BundleTools
import qbs.Environment
import qbs.File
import qbs.FileInfo
import qbs.DarwinTools
import qbs.ModUtils
import qbs.Probes
import qbs.PropertyList
import qbs.Utilities

Module {
    id: xcodeModule

    Probes.XcodeLocationProbe {
        id: xcodeLocationProbe
        condition: !xcodeModule.developerPath
    }

    Probes.XcodeProbe {
        id: xcodeProbe
        developerPath: xcodeModule.developerPath
        platformType: xcodeModule.platformType
        platformPath: xcodeModule.platformPath
        devicePlatformPath: xcodeModule.devicePlatformPath
        xcodebuildPath: xcodeModule.xcodebuildPath
        sdksPath: xcodeModule.sdksPath
    }

    condition: qbs.targetOS.includes("darwin") &&
               qbs.toolchain && qbs.toolchain.includes("xcode")

    version: xcodeProbe.xcodeVersion

    property path developerPath: xcodeLocationProbe.found
                                 ? xcodeLocationProbe.developerPath
                                 : undefined
    property string sdk: DarwinTools.applePlatformName(qbs.targetOS, platformType)
    property stringList targetDevices: DarwinTools.targetDevices(qbs.targetOS)

    property string platformType: {
        if (qbs.targetOS.containsAny(["ios-simulator", "tvos-simulator", "watchos-simulator"]))
            return "simulator";
        if (qbs.targetOS.containsAny(["ios", "tvos", "watchos"]))
            return "device";
    }

    readonly property string sdkName: {
        if (_sdkSettings) {
            return _sdkSettings["CanonicalName"];
        }
    }

    readonly property string sdkVersion: {
        if (_sdkSettings) {
            return _sdkSettings["Version"];
        }
    }
    readonly property string shortSdkVersion: {
        var v = sdkVersion;
        if (v && v.split('.').length > 2)
            v = v.slice(0, v.lastIndexOf('.'));
        return v;
    }

    readonly property string latestSdkName: {
        if (_latestSdk) {
            return _latestSdk["CanonicalName"];
        }
    }

    readonly property string latestSdkVersion: {
        if (_latestSdk) {
            return _latestSdk["Version"];
        }
    }

    readonly property stringList availableSdkNames: {
        if (_availableSdks) {
            return _availableSdks.map(function (obj) { return obj["CanonicalName"]; });
        }
    }

    readonly property stringList availableSdkVersions: {
        if (_availableSdks) {
            return _availableSdks.map(function (obj) { return obj["Version"]; });
        }
    }

    readonly property path toolchainPath: FileInfo.joinPaths(toolchainsPath,
                                                             "XcodeDefault" + ".xctoolchain")
    readonly property path platformPath: FileInfo.joinPaths(platformsPath,
                                                            DarwinTools.applePlatformDirectoryName(
                                                                qbs.targetOS, platformType)
                                                            + ".platform")
    readonly property path devicePlatformPath: FileInfo.joinPaths(
                                                   platformsPath,
                                                   DarwinTools.applePlatformDirectoryName(
                                                       qbs.targetOS, "device")
                                                   + ".platform")
    readonly property path simulatorPlatformPath: FileInfo.joinPaths(
                                                      platformsPath,
                                                      DarwinTools.applePlatformDirectoryName(
                                                          qbs.targetOS, "simulator")
                                                      + ".platform")
    readonly property path sdkPath: FileInfo.joinPaths(sdksPath,
                                                       DarwinTools.applePlatformDirectoryName(
                                                           qbs.targetOS, platformType,
                                                           shortSdkVersion) + ".sdk")

    // private properties
    readonly property path toolchainsPath: FileInfo.joinPaths(developerPath, "Toolchains")
    readonly property path platformsPath: FileInfo.joinPaths(developerPath, "Platforms")
    readonly property path sdksPath: FileInfo.joinPaths(platformPath, "Developer", "SDKs")

    readonly property path platformInfoPlist: FileInfo.joinPaths(platformPath, "Info.plist")
    readonly property path sdkSettingsPlist: FileInfo.joinPaths(sdkPath, "SDKSettings.plist")
    readonly property path toolchainInfoPlist: FileInfo.joinPaths(toolchainPath,
                                                                  "ToolchainInfo.plist")

    readonly property var _platformSettings: xcodeProbe.platformSettings

    readonly property var _platformProps: {
        if (_platformSettings)
            return _platformSettings["DefaultProperties"];
    }

    readonly property stringList standardArchitectures: _architectureSettings["ARCHS_STANDARD"]

    readonly property var _architectureSettings: xcodeProbe.architectureSettings

    readonly property var _availableSdks: xcodeProbe.availableSdks

    readonly property var _latestSdk: _availableSdks[_availableSdks.length - 1]

    readonly property var _sdkSettings: {
        if (_availableSdks) {
            // see 'sdk' property doc to understand why this loop is needed
            for (var i in _availableSdks) {
                if (_availableSdks[i]["Version"] === sdk)
                    return _availableSdks[i];
                if (_availableSdks[i]["CanonicalName"] === sdk)
                    return _availableSdks[i];
            }

            // Latest SDK available for the platform (default case)
            if (DarwinTools.applePlatformName(qbs.targetOS, platformType) === sdk)
                return _latestSdk;
        }
    }

    readonly property var _sdkProps: {
        if (_sdkSettings) {
            return _sdkSettings["DefaultProperties"];
        }
    }

    qbs.sysroot: sdkPath

    validate: {
        if (!_availableSdks) {
            throw "There are no SDKs available for this platform in the Xcode installation.";
        }

        if (!_sdkSettings) {
            throw "There is no matching SDK available for " + sdk + ".";
        }

        var validator = new ModUtils.PropertyValidator("xcode");
        validator.setRequiredProperty("developerPath", developerPath);
        validator.setRequiredProperty("sdk", sdk);
        validator.setRequiredProperty("sdkName", sdkName);
        validator.setRequiredProperty("sdkVersion", sdkVersion);
        validator.setRequiredProperty("toolchainsPath", toolchainsPath);
        validator.setRequiredProperty("toolchainPath", toolchainPath);
        validator.setRequiredProperty("platformsPath", platformsPath);
        validator.setRequiredProperty("platformPath", platformPath);
        validator.setRequiredProperty("sdksPath", sdkPath);
        validator.setRequiredProperty("sdkPath", sdkPath);
        validator.addVersionValidator("sdkVersion", sdkVersion, 2, 3);
        validator.addCustomValidator("sdkName", sdkName, function (value) {
            return value === DarwinTools.applePlatformDirectoryName(
                        qbs.targetOS, platformType, shortSdkVersion, false).toLowerCase();
        }, "is '" + sdkName + "', but target OS is [" + qbs.targetOS.join(",")
        + "] and Xcode SDK version is '" + sdkVersion + "'");
        validator.addCustomValidator("sdk", sdk, function (value) {
            return value === sdkName || (value + shortSdkVersion) === sdkName;
        }, "is '" + sdk + "', but canonical SDK name is '" + sdkName + "'");
        validator.validate();
    }

    property var buildEnv: ({
        "DEVELOPER_DIR": developerPath,
        "SDKROOT": sdkPath
    })

    setupBuildEnvironment: {
        var v = new ModUtils.EnvironmentVariable("PATH", FileInfo.pathListSeparator(), false);
        v.prepend(product.xcode.platformPath + "/Developer/usr/bin");
        v.prepend(product.xcode.developerPath + "/usr/bin");
        v.set();

        for (var key in product.xcode.buildEnv) {
            v = new ModUtils.EnvironmentVariable(key);
            v.value = product.xcode.buildEnv[key];
            v.set();
        }
    }
}
