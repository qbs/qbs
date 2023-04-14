/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Copyright (C) 2021 Ivan Komissarov (abbapoh@gmail.com)
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

import qbs.BundleTools
import qbs.DarwinTools
import qbs.Environment
import qbs.File
import qbs.FileInfo
import qbs.Host
import qbs.ModUtils
import qbs.PropertyList
import qbs.Probes
import qbs.Utilities
import "codesign.js" as CodeSign
import "../xcode/xcode.js" as XcodeUtils

CodeSignModule {
    Depends { name: "xcode"; required: qbs.toolchain && qbs.toolchain.includes("xcode") }

    Probes.BinaryProbe {
        id: codesignProbe
        names: [codesignName]
    }

    condition: Host.os().includes("macos") && qbs.targetOS.includes("darwin")
    priority: 0

    enableCodeSigning: _codeSigningRequired

    codesignName: "codesign"
    codesignPath: codesignProbe.filePath

    _canSignArtifacts: true

    property string signingType: {
        if (_adHocCodeSigningAllowed)
            return "ad-hoc";
        if (_codeSigningAllowed)
            return "app-store";
    }

    PropertyOptions {
        name: "signingType"
        allowedValues: ["app-store", "apple-id", "ad-hoc"]
    }

    property string signingIdentity: {
        if (signingType === "ad-hoc") // only useful on macOS
            return "-";

        var isDebug = qbs.buildVariant !== "release";

        if (qbs.targetOS.includes("ios") || qbs.targetOS.includes("tvos")
                || qbs.targetOS.includes("watchos")) {
            switch (signingType) {
            case "app-store":
                return isDebug ? "iPhone Developer" : "iPhone Distribution";
            }
        }

        if (qbs.targetOS.includes("macos")) {
            switch (signingType) {
            case "app-store":
                return isDebug ? "Mac Developer" : "3rd Party Mac Developer Application";
            case "apple-id":
                return "Developer ID Application";
            }
        }
    }

    signingTimestamp: "none"

    property string provisioningProfile
    PropertyOptions {
        name: "provisioningProfile"
        description: "Name or UUID of the provisioning profile to embed in the application; " +
                     "typically left blank to allow automatic provisioning"
    }

    property string teamIdentifier
    PropertyOptions {
        name: "teamIdentifier"
        description: "Name or identifier of the development team whose identities will be used; " +
                     "typically left blank unless signed into multiple development teams"
    }

    property path provisioningProfilesPath: "~/Library/MobileDevice/Provisioning Profiles"

    readonly property var _actualSigningIdentity: {
        if (signingIdentity === "-") {
            return {
                SHA1: signingIdentity,
                subjectInfo: { CN: "ad hoc" }
            }
        }

        var identities = CodeSign.findSigningIdentities(signingIdentity, teamIdentifier);
        if (identities && Object.keys(identities).length > 1) {
            throw "Multiple codesigning identities (i.e. certificate and private key pairs) " +
                    "matching '" + signingIdentity + "' were found." +
                    CodeSign.humanReadableIdentitySummary(identities);
        }

        for (var i in identities)
            return identities[i];
    }

    // Allowed for macOS
    readonly property bool _adHocCodeSigningAllowed:
        XcodeUtils.boolFromSdkOrPlatform("AD_HOC_CODE_SIGNING_ALLOWED",
                                         xcode._sdkProps, xcode._platformProps, true)

    // Allowed for all device platforms (not simulators)
    readonly property bool _codeSigningAllowed:
        XcodeUtils.boolFromSdkOrPlatform("CODE_SIGNING_ALLOWED",
                                         xcode._sdkProps, xcode._platformProps, true)

    // Required for tvOS, iOS, and watchOS (not simulators)
    property bool _codeSigningRequired: {
        // allow to override value from Xcode so tests do not require signing
        var envRequired = Environment.getEnv("QBS_AUTOTEST_CODE_SIGNING_REQUIRED");
        if (envRequired)
            return envRequired === "1";
        return XcodeUtils.boolFromSdkOrPlatform("CODE_SIGNING_REQUIRED",
                                         xcode._sdkProps, xcode._platformProps, false)
    }

    // Required for tvOS, iOS, and watchOS (not simulators)
    readonly property bool _entitlementsRequired:
        XcodeUtils.boolFromSdkOrPlatform("ENTITLEMENTS_REQUIRED",
                                         xcode._sdkProps, xcode._platformProps, false)

    readonly property bool _provisioningProfileAllowed:
        product.bundle
        && product.bundle.isBundle
        && product.type.includes("application")
        && xcode.platformType !== "simulator"

    // Required for tvOS, iOS, and watchOS (not simulators)
    // PROVISIONING_PROFILE_REQUIRED is specified only in Embedded-Device.xcspec in the
    // IDEiOSSupportCore IDE plugin, so we'll just write out the logic here manually
    readonly property bool _provisioningProfileRequired:
        _provisioningProfileAllowed && !qbs.targetOS.includes("macos")

    // Not used on simulator platforms either but provisioning profiles aren't used there anyways
    readonly property string _provisioningProfilePlatform: {
        if (qbs.targetOS.includes("macos"))
            return "OSX";
        if (qbs.targetOS.includes("ios") || qbs.targetOS.includes("watchos"))
            return "iOS";
        if (qbs.targetOS.includes("tvos"))
            return "tvOS";
    }

    readonly property string _embeddedProfileName:
        (xcode._platformProps || {})["EMBEDDED_PROFILE_NAME"] || "embedded.mobileprovision"

    setupBuildEnvironment: {
        var prefixes = product.xcode ? [
            product.xcode.platformPath + "/Developer",
            product.xcode.toolchainPath,
            product.xcode.developerPath
        ] : [];
        for (var i = 0; i < prefixes.length; ++i) {
            var codesign_allocate = prefixes[i] + "/usr/bin/codesign_allocate";
            if (File.exists(codesign_allocate)) {
                var v = new ModUtils.EnvironmentVariable("CODESIGN_ALLOCATE");
                v.value = codesign_allocate;
                v.set();
                break;
            }
        }
    }

    Group {
        name: "Provisioning Profiles"
        prefix: codesign.provisioningProfilesPath + "/"
        files: ["*.mobileprovision", "*.provisionprofile"]
    }

    FileTagger {
        fileTags: ["codesign.entitlements"]
        patterns: ["*.entitlements"]
    }

    FileTagger {
        fileTags: ["codesign.provisioningprofile"]
        patterns: ["*.mobileprovision", "*.provisionprofile"]
    }

    Rule {
        multiplex: true
        condition: product.codesign.enableCodeSigning &&
                   product.codesign._provisioningProfileAllowed
        inputs: ["codesign.provisioningprofile"]

        outputFileTags: ["codesign.embedded_provisioningprofile"]
        outputArtifacts: {
            var artifacts = [];
            var provisioningProfiles = (inputs["codesign.provisioningprofile"] || [])
                .map(function (a) { return a.filePath; });
            var bestProfile = CodeSign.findBestProvisioningProfile(product, provisioningProfiles);
            var uuid = product.provisioningProfile;
            if (bestProfile) {
                artifacts.push({
                    filePath: FileInfo.joinPaths(product.destinationDirectory,
                                                 product.codesign._embeddedProfileName),
                    fileTags: ["codesign.embedded_provisioningprofile"],
                    codesign: {
                        _provisioningProfileFilePath: bestProfile.filePath,
                        _provisioningProfileData: JSON.stringify(bestProfile.data),
                    }
                });
            } else if (uuid) {
                throw "Your build settings specify a provisioning profile with the UUID '"
                        + uuid + "', however, no such provisioning profile was found.";
            } else if (product._provisioningProfileRequired) {
                var hasProfiles = !!((inputs["codesign.provisioningprofile"] || []).length);
                var teamIdentifier = product.teamIdentifier;
                var codeSignIdentity = product.signingIdentity;
                if (hasProfiles) {
                    if (codeSignIdentity) {
                        console.warn("No provisioning profiles matching the bundle identifier '"
                                     + product.bundle.identifier
                                     + "' were found.");
                    } else {
                        console.warn("No provisioning profiles matching an applicable signing "
                                     + "identity were found.");
                    }
                } else {
                    if (codeSignIdentity) {
                        if (teamIdentifier) {
                            console.warn("No provisioning profiles with a valid signing identity "
                                         + "(i.e. certificate and private key pair) matching the "
                                         + "team ID '" + teamIdentifier + "' were found.")
                        } else {
                            console.warn("No provisioning profiles with a valid signing identity "
                                         + "(i.e. certificate and private key pair) were found.");
                        }
                     } else {
                        console.warn("No non-expired provisioning profiles were found.");
                    }
                }
            }
            return artifacts;
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            var data = JSON.parse(output.codesign._provisioningProfileData);
            cmd.source = output.codesign._provisioningProfileFilePath;
            cmd.destination = output.filePath;
            cmd.description = "using provisioning profile " + data.Name + " (" + data.UUID + ")";
            cmd.highlight = "filegen";
            cmd.sourceCode = function() {
                File.copy(source, destination);
            };
            return [cmd];
        }
    }

    Rule {
        multiplex: true
        condition: product.codesign.enableCodeSigning
        inputs: ["codesign.entitlements", "codesign.embedded_provisioningprofile"]

        Artifact {
            filePath: FileInfo.joinPaths(product.destinationDirectory,
                                         product.targetName + ".xcent")
            fileTags: ["codesign.xcent"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating entitlements";
            cmd.highlight = "codegen";
            cmd.bundleIdentifier = product.bundle.identifier;
            cmd.signingEntitlements = (inputs["codesign.entitlements"] || [])
                .map(function (a) { return a.filePath; });
            cmd.provisioningProfiles = (inputs["codesign.embedded_provisioningprofile"] || [])
                .map(function (a) { return a.filePath; });
            cmd.platformPath = product.xcode ? product.xcode.platformPath : undefined;
            cmd.sdkPath = product.xcode ? product.xcode.sdkPath : undefined;
            cmd.sourceCode = function() {
                var i;
                var provData = {};
                var provisionProfiles = inputs["codesign.embedded_provisioningprofile"];
                for (i in provisionProfiles) {
                    var plist = new PropertyList();
                    try {
                        plist.readFromData(Utilities.smimeMessageContent(
                                               provisionProfiles[i].filePath));
                        provData = plist.toObject();
                    } finally {
                        plist.clear();
                    }
                }

                var aggregateEntitlements = {};

                // Start building up an aggregate entitlements plist from the files in the SDKs,
                // which contain placeholders in the same manner as Info.plist
                function entitlementsFileContents(path) {
                    return File.exists(path) ? BundleTools.infoPlistContents(path) : undefined;
                }
                var entitlementsSources = [];
                if (platformPath) {
                    entitlementsSources.push(
                            entitlementsFileContents(
                                    FileInfo.joinPaths(platformPath, "Entitlements.plist")));
                }
                if (sdkPath) {
                    entitlementsSources.push(
                            entitlementsFileContents(
                                    FileInfo.joinPaths(sdkPath, "Entitlements.plist")));
                }

                for (i = 0; i < signingEntitlements.length; ++i) {
                    entitlementsSources.push(entitlementsFileContents(signingEntitlements[i]));
                }

                for (i = 0; i < entitlementsSources.length; ++i) {
                    var contents = entitlementsSources[i];
                    for (var key in contents) {
                        if (contents.hasOwnProperty(key))
                            aggregateEntitlements[key] = contents[key];
                    }
                }

                contents = provData["Entitlements"];
                for (key in contents) {
                    if (contents.hasOwnProperty(key) && !aggregateEntitlements.hasOwnProperty(key))
                        aggregateEntitlements[key] = contents[key];
                }

                // Expand entitlements variables with data from the provisioning profile
                var env = {
                    "AppIdentifierPrefix": (provData["ApplicationIdentifierPrefix"] || "") + ".",
                    "CFBundleIdentifier": bundleIdentifier
                };
                DarwinTools.expandPlistEnvironmentVariables(aggregateEntitlements, env, true);

                // Anything with an undefined or otherwise empty value should be removed
                // Only JSON-formatted plists can have null values, other formats error out
                // This also follows Xcode behavior
                DarwinTools.cleanPropertyList(aggregateEntitlements);

                var plist = new PropertyList();
                try {
                    plist.readFromObject(aggregateEntitlements);
                    plist.writeToFile(outputs["codesign.xcent"][0].filePath, "xml1");
                } finally {
                    plist.clear();
                }
            };
            return [cmd];
        }
    }
}
