/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
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
import qbs.DarwinTools
import qbs.FileInfo
import qbs.ModUtils
import qbs.PropertyList
import qbs.TextFile

UnixGCC {
    condition: false

    Depends { name: "xcode"; required: qbs.toolchain && qbs.toolchain.contains("xcode") }

    targetVendor: "apple"
    targetSystem: "darwin"
    targetAbi: "macho"
    imageFormat: "macho"

    compilerDefines: ["__GNUC__=4", "__APPLE__"]
    cxxStandardLibrary: qbs.toolchain.contains("clang") && cxxLanguageVersion !== "c++98"
                        ? "libc++" : base
    loadableModulePrefix: ""
    loadableModuleSuffix: ".bundle"
    dynamicLibrarySuffix: ".dylib"
    separateDebugInformation: true
    debugInfoBundleSuffix: ".dSYM"
    debugInfoSuffix: ".dwarf"

    toolchainInstallPath: xcode.present
                          ? FileInfo.joinPaths(xcode.toolchainPath, "usr", "bin") : base
    sysroot: xcode.present ? xcode.sdkPath : base

    setupBuildEnvironment: {
        for (var key in buildEnv) {
            v = new ModUtils.EnvironmentVariable(key);
            v.value = buildEnv[key];
            v.set();
        }
    }

    property var defaultInfoPlist: {
        var dict = {};

        if (qbs.targetOS.contains("macos")) {
            dict["NSPrincipalClass"] = "NSApplication"; // needed for Retina display support

            if (minimumMacosVersion)
                dict["LSMinimumSystemVersion"] = minimumMacosVersion;
        }

        if (qbs.targetOS.containsAny(["ios", "tvos"])) {
            dict["LSRequiresIPhoneOS"] = true;

            if (xcode.platformType === "device") {
                if (qbs.targetOS.contains("ios"))
                    dict["UIRequiredDeviceCapabilities"] = ["armv7"];

                if (qbs.targetOS.contains("tvos"))
                    dict["UIRequiredDeviceCapabilities"] = ["arm64"];
            }
        }

        if (xcode.present) {
            var targetDevices = DarwinTools.targetedDeviceFamily(xcode.targetDevices);
            if (qbs.targetOS.contains("ios"))
                dict["UIDeviceFamily"] = targetDevices;

            if (qbs.targetOS.containsAny(["ios", "watchos"])) {
                var orientations = [
                    "UIInterfaceOrientationPortrait",
                    "UIInterfaceOrientationPortraitUpsideDown",
                    "UIInterfaceOrientationLandscapeLeft",
                    "UIInterfaceOrientationLandscapeRight"
                ];

                if (targetDevices.contains("ipad"))
                    dict["UISupportedInterfaceOrientations~ipad"] = orientations;

                if (targetDevices.contains("watch"))
                    dict["UISupportedInterfaceOrientations"] = orientations.slice(0, 2);

                if (targetDevices.contains("iphone")) {
                    orientations.splice(1, 1);
                    dict["UISupportedInterfaceOrientations"] = orientations;
                }
            }
        }

        return dict;
    }

    // private properties
    readonly property var buildEnv: {
        var env = {
            "ARCHS_STANDARD": targetArch, // TODO: this will be affected by multi-arch support
            "EXECUTABLE_NAME": product.targetName,
            "LANG": "en_US.US-ASCII",
            "PRODUCT_NAME": product.name
        }

        // Set the corresponding environment variable even if the minimum OS version is undefined,
        // because this indicates the default deployment target for that OS
        if (qbs.targetOS.contains("ios"))
            env["IPHONEOS_DEPLOYMENT_TARGET"] = minimumIosVersion || "";
        if (qbs.targetOS.contains("macos"))
            env["MACOSX_DEPLOYMENT_TARGET"] = minimumMacosVersion || "";
        if (qbs.targetOS.contains("watchos"))
            env["WATCHOS_DEPLOYMENT_TARGET"] = minimumWatchosVersion || "";
        if (qbs.targetOS.contains("tvos"))
            env["TVOS_DEPLOYMENT_TARGET"] = minimumTvosVersion || "";

        if (xcode.present)
            env["TARGETED_DEVICE_FAMILY"] = DarwinTools.targetedDeviceFamily(xcode.targetDevices);
        return env;
    }

    property string minimumDarwinVersion
    property string minimumDarwinVersionCompilerFlag
    property string minimumDarwinVersionLinkerFlag

    Rule {
        condition: qbs.targetOS.contains("darwin")
        inputs: ["qbs"]

        Artifact {
            filePath: product.name + "-cpp-Info.plist"
            fileTags: ["partial_infoplist"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.inputData = ModUtils.moduleProperty(product, "defaultInfoPlist");
            cmd.outputFilePath = output.filePath;
            cmd.sourceCode = function() {
                var plist = new PropertyList();
                try {
                    plist.readFromObject(inputData);
                    plist.writeToFile(outputFilePath, "xml1");
                } finally {
                    plist.clear();
                }
            };
            return [cmd];
        }
    }
}
