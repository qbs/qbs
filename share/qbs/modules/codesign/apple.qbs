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
        prefix: provisioningProfilesPath + "/"
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

    Group {
        condition: enableCodeSigning

        Rule {
            multiplex: true
            inputs: ["codesign.entitlements", "codesign.embedded_provisioningprofile"]

            Artifact {
                filePath: FileInfo.joinPaths(product.destinationDirectory,
                                             product.targetName + ".xcent")
                fileTags: ["codesign.xcent"]
            }

            prepare: CodeSign.generateAppleEntitlementsCommands(CodeSign, arguments)
        }

        Rule {
            multiplex: true
            condition: module._provisioningProfileAllowed
            inputs: ["codesign.provisioningprofile"]
            outputFileTags: ["codesign.embedded_provisioningprofile"]
            outputArtifacts: CodeSign.generateAppleProvisioningProfileOutputs(product, inputs)
            prepare: CodeSign.generateAppleProvisioningProfileCommands.apply(CodeSign, arguments)
        }
    }
}
