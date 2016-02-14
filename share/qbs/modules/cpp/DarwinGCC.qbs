/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

import qbs
import qbs.ModUtils

UnixGCC {
    condition: false

    compilerDefines: ["__GNUC__=4", "__APPLE__"]
    loadableModulePrefix: ""
    loadableModuleSuffix: ".bundle"
    dynamicLibrarySuffix: ".dylib"
    separateDebugInformation: true
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
