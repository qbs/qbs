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

import qbs.BundleTools
import qbs.DarwinTools
import qbs.Environment
import qbs.File
import qbs.FileInfo
import qbs.Host
import qbs.ModUtils
import qbs.PropertyList
import qbs.TextFile
import qbs.Utilities
import "bundle.js" as Bundle
import "../codesign/codesign.js" as Codesign

Module {
    Depends { name: "xcode"; required: false; }
    Depends { name: "codesign"; required: false; }

    Probe {
        id: bundleSettingsProbe
        condition: qbs.targetOS.includes("darwin")

        property string xcodeDeveloperPath: xcode.developerPath
        property var xcodeArchSettings: xcode._architectureSettings
        property string productTypeIdentifier: _productTypeIdentifier
        property bool useXcodeBuildSpecs: !useBuiltinXcodeBuildSpecs
        property bool isMacOs: qbs.targetOS.includes("macos")
        property bool xcodePresent: xcode.present
        property string xcodeVersion: xcode.version

        // Note that we include several settings pointing to properties which reference the output
        // of this probe (WRAPPER_NAME, WRAPPER_EXTENSION, etc.). This is to ensure that derived
        // properties take into account the value of these settings if the user customized them.
        property var additionalSettings: ({
            "DEVELOPMENT_LANGUAGE": "English",
            "EXECUTABLE_VARIANT_SUFFIX": "", // e.g. _debug, _profile
            "FRAMEWORK_VERSION": frameworkVersion,
            "GENERATE_PKGINFO_FILE": generatePackageInfo !== undefined
                                     ? (generatePackageInfo ? "YES" : "NO")
                                     : undefined,
            "IS_MACCATALYST": "NO",
            "LD_RUNPATH_SEARCH_PATHS_NO": [],
            "PRODUCT_NAME": product.targetName,
            "LOCAL_APPS_DIR": Environment.getEnv("HOME") + "/Applications",
            "LOCAL_LIBRARY_DIR": Environment.getEnv("HOME") + "/Library",
            // actually, this is cpp.targetAbi, but XCode does not set it for non-simulator builds
            // while Qbs set it to "macho".
            "LLVM_TARGET_TRIPLE_SUFFIX": qbs.targetOS.includes("simulator") ? "-simulator" : "",
            "SWIFT_PLATFORM_TARGET_PREFIX": isMacOs ? "macos"
                                                    : qbs.targetOS.includes("ios") ? "ios" : "",
            "TARGET_BUILD_DIR": product.buildDirectory,
            "WRAPPER_NAME": bundleName,
            "WRAPPER_EXTENSION": extension
        })

        // Outputs
        property var xcodeSettings: ({})
        property var productTypeIdentifierChain: []

        configure: {
            var specsPaths = [path];
            var specsSeparator = "-";
            if (xcodeDeveloperPath && useXcodeBuildSpecs) {
                specsPaths = Bundle.macOSSpecsPaths(xcodeVersion, xcodeDeveloperPath);
                specsSeparator = " ";
            }

            var reader = new Bundle.XcodeBuildSpecsReader(specsPaths,
                                                          specsSeparator,
                                                          additionalSettings,
                                                          !isMacOs);
            var settings = reader.expandedSettings(productTypeIdentifier,
                                                   xcodePresent
                                                   ? xcodeArchSettings
                                                   : {});
            var chain = reader.productTypeIdentifierChain(productTypeIdentifier);
            if (settings && chain) {
                xcodeSettings = settings;
                productTypeIdentifierChain = chain;
                found = true;
            } else {
                xcodeSettings = {};
                productTypeIdentifierChain = [];
                found = false;
            }
        }
    }

    additionalProductTypes: !(product.multiplexed || product.aggregate)
                            || !product.multiplexConfigurationId ? ["bundle.content"] : []

    property bool isBundle: !product.consoleApplication && qbs.targetOS.includes("darwin")

    readonly property bool isShallow: bundleSettingsProbe.xcodeSettings["SHALLOW_BUNDLE"] === "YES"

    property string identifierPrefix: "org.example"
    property string identifier: [identifierPrefix, Utilities.rfc1034Identifier(product.targetName)].join(".")

    property string extension: bundleSettingsProbe.xcodeSettings["WRAPPER_EXTENSION"]

    property string packageType: Bundle.packageType(_productTypeIdentifier)

    property string signature: "????" // legacy creator code in Mac OS Classic (CFBundleSignature), can be ignored

    property string bundleName: bundleSettingsProbe.xcodeSettings["WRAPPER_NAME"]

    property string frameworkVersion: {
        var n = parseInt(product.version, 10);
        return isNaN(n) ? bundleSettingsProbe.xcodeSettings["FRAMEWORK_VERSION"] : String(n);
    }

    property bool generatePackageInfo: {
        // Make sure to return undefined as default to indicate "not set"
        var genPkgInfo = bundleSettingsProbe.xcodeSettings["GENERATE_PKGINFO_FILE"];
        if (genPkgInfo)
            return genPkgInfo === "YES";
    }

    property pathList publicHeaders
    property pathList privateHeaders
    property pathList resources

    property var infoPlist
    property bool processInfoPlist: true
    property bool embedInfoPlist: product.consoleApplication && !isBundle
    property string infoPlistFormat: qbs.targetOS.includes("macos") ? "same-as-input" : "binary1"

    property string localizedResourcesFolderSuffix: ".lproj"

    property string lsregisterName: "lsregister"
    property string lsregisterPath: FileInfo.joinPaths(
                                        "/System/Library/Frameworks/CoreServices.framework" +
                                        "/Versions/A/Frameworks/LaunchServices.framework" +
                                        "/Versions/A/Support", lsregisterName);

    // all paths are relative to the directory containing the bundle
    readonly property string infoPlistPath: bundleSettingsProbe.xcodeSettings["INFOPLIST_PATH"]
    readonly property string infoStringsPath: bundleSettingsProbe.xcodeSettings["INFOSTRINGS_PATH"]
    readonly property string pbdevelopmentPlistPath: bundleSettingsProbe.xcodeSettings["PBDEVELOPMENTPLIST_PATH"]
    readonly property string pkgInfoPath: bundleSettingsProbe.xcodeSettings["PKGINFO_PATH"]
    readonly property string versionPlistPath: bundleSettingsProbe.xcodeSettings["VERSIONPLIST_PATH"]

    readonly property string executablePath: bundleSettingsProbe.xcodeSettings["EXECUTABLE_PATH"]

    readonly property string contentsFolderPath: bundleSettingsProbe.xcodeSettings["CONTENTS_FOLDER_PATH"]
    readonly property string documentationFolderPath: bundleSettingsProbe.xcodeSettings["DOCUMENTATION_FOLDER_PATH"]
    readonly property string executableFolderPath: bundleSettingsProbe.xcodeSettings["EXECUTABLE_FOLDER_PATH"]
    readonly property string executablesFolderPath: bundleSettingsProbe.xcodeSettings["EXECUTABLES_FOLDER_PATH"]
    readonly property string frameworksFolderPath: bundleSettingsProbe.xcodeSettings["FRAMEWORKS_FOLDER_PATH"]
    readonly property string javaFolderPath: bundleSettingsProbe.xcodeSettings["JAVA_FOLDER_PATH"]
    readonly property string localizedResourcesFolderPath: bundleSettingsProbe.xcodeSettings["LOCALIZED_RESOURCES_FOLDER_PATH"]
    readonly property string pluginsFolderPath: bundleSettingsProbe.xcodeSettings["PLUGINS_FOLDER_PATH"]
    readonly property string privateHeadersFolderPath: bundleSettingsProbe.xcodeSettings["PRIVATE_HEADERS_FOLDER_PATH"]
    readonly property string publicHeadersFolderPath: bundleSettingsProbe.xcodeSettings["PUBLIC_HEADERS_FOLDER_PATH"]
    readonly property string scriptsFolderPath: bundleSettingsProbe.xcodeSettings["SCRIPTS_FOLDER_PATH"]
    readonly property string sharedFrameworksFolderPath: bundleSettingsProbe.xcodeSettings["SHARED_FRAMEWORKS_FOLDER_PATH"]
    readonly property string sharedSupportFolderPath: bundleSettingsProbe.xcodeSettings["SHARED_SUPPORT_FOLDER_PATH"]
    readonly property string unlocalizedResourcesFolderPath: bundleSettingsProbe.xcodeSettings["UNLOCALIZED_RESOURCES_FOLDER_PATH"]
    readonly property string versionsFolderPath: bundleSettingsProbe.xcodeSettings["VERSIONS_FOLDER_PATH"]

    property bool useBuiltinXcodeBuildSpecs: !_useXcodeBuildSpecs // true to use ONLY the qbs build specs

    // private properties
    property string _productTypeIdentifier: Bundle.productTypeIdentifier(product.type)
    property stringList _productTypeIdentifierChain: bundleSettingsProbe.productTypeIdentifierChain

    readonly property path _developerPath: xcode.developerPath
    readonly property path _platformInfoPlist: xcode.platformInfoPlist
    readonly property path _sdkSettingsPlist: xcode.sdkSettingsPlist
    readonly property path _toolchainInfoPlist: xcode.toolchainInfoPlist

    property bool _useXcodeBuildSpecs: true // TODO: remove in 1.25

    property var extraEnv: ({
        "PRODUCT_BUNDLE_IDENTIFIER": identifier
    })

    readonly property var qmakeEnv: {
        return {
            "BUNDLEIDENTIFIER": identifier,
            "EXECUTABLE": product.targetName,
            "FULL_VERSION": product.version || "1.0", // CFBundleVersion
            "ICON": product.targetName, // ### QBS-73
            "LIBRARY": product.targetName,
            "SHORT_VERSION": product.version || "1.0", // CFBundleShortVersionString
            "TYPEINFO": signature // CFBundleSignature
        };
    }

    readonly property var defaultInfoPlist: {
        return {
            CFBundleDevelopmentRegion: "en", // default localization
            CFBundleDisplayName: product.targetName, // localizable
            CFBundleExecutable: product.targetName,
            CFBundleIdentifier: identifier,
            CFBundleInfoDictionaryVersion: "6.0",
            CFBundleName: product.targetName, // short display name of the bundle, localizable
            CFBundlePackageType: packageType,
            CFBundleShortVersionString: product.version || "1.0", // "release" version number, localizable
            CFBundleSignature: signature, // legacy creator code in Mac OS Classic, can be ignored
            CFBundleVersion: product.version || "1.0.0" // build version number, must be 3 octets
        };
    }

    validate: {
        if (!qbs.targetOS.includes("darwin"))
            return;
        if (!bundleSettingsProbe.found) {
            var error = "Bundle product type " + _productTypeIdentifier + " is not supported.";
            if ((_productTypeIdentifier || "").startsWith("com.apple.product-type."))
                error += " You may need to upgrade Xcode.";
            throw error;
        }

        var validator = new ModUtils.PropertyValidator("bundle");
        validator.setRequiredProperty("bundleName", bundleName);
        validator.setRequiredProperty("infoPlistPath", infoPlistPath);
        validator.setRequiredProperty("pbdevelopmentPlistPath", pbdevelopmentPlistPath);
        validator.setRequiredProperty("pkgInfoPath", pkgInfoPath);
        validator.setRequiredProperty("versionPlistPath", versionPlistPath);
        validator.setRequiredProperty("executablePath", executablePath);
        validator.setRequiredProperty("contentsFolderPath", contentsFolderPath);
        validator.setRequiredProperty("documentationFolderPath", documentationFolderPath);
        validator.setRequiredProperty("executableFolderPath", executableFolderPath);
        validator.setRequiredProperty("executablesFolderPath", executablesFolderPath);
        validator.setRequiredProperty("frameworksFolderPath", frameworksFolderPath);
        validator.setRequiredProperty("javaFolderPath", javaFolderPath);
        validator.setRequiredProperty("localizedResourcesFolderPath", localizedResourcesFolderPath);
        validator.setRequiredProperty("pluginsFolderPath", pluginsFolderPath);
        validator.setRequiredProperty("privateHeadersFolderPath", privateHeadersFolderPath);
        validator.setRequiredProperty("publicHeadersFolderPath", publicHeadersFolderPath);
        validator.setRequiredProperty("scriptsFolderPath", scriptsFolderPath);
        validator.setRequiredProperty("sharedFrameworksFolderPath", sharedFrameworksFolderPath);
        validator.setRequiredProperty("sharedSupportFolderPath", sharedSupportFolderPath);
        validator.setRequiredProperty("unlocalizedResourcesFolderPath", unlocalizedResourcesFolderPath);

        if (packageType === "FMWK") {
            validator.setRequiredProperty("frameworkVersion", frameworkVersion);
            validator.setRequiredProperty("versionsFolderPath", versionsFolderPath);
        }

        // extension and infoStringsPath might not be set
        return validator.validate();
    }

    FileTagger {
        fileTags: ["infoplist"]
        patterns: ["Info.plist", "*-Info.plist"]
    }

    Rule {
        condition: qbs.targetOS.includes("darwin")
        multiplex: true
        requiresInputs: false // TODO: The resources property should probably be a tag instead.
        inputs: ["infoplist", "partial_infoplist"]
        outputFileTags: ["bundle.input", "aggregate_infoplist"]
        outputArtifacts: Bundle.aggregateInfoPlistOutputs(product)
        prepare: Bundle.aggregateInfoPlistCommands.apply(Bundle, arguments)
    }

    Rule {
        condition: qbs.targetOS.includes("darwin")
        multiplex: true
        inputs: ["aggregate_infoplist"]
        outputFileTags: ["bundle.input", "pkginfo"]
        outputArtifacts: Bundle.generatePkgInfoOutputs(product)
        prepare: Bundle.generatePkgInfoCommands.apply(Bundle, arguments)
    }

    Rule {
        condition: qbs.targetOS.includes("darwin")
        multiplex: true
        inputs: ["bundle.input",
                 "aggregate_infoplist", "pkginfo", "hpp",
                 "icns", "codesign.xcent",
                 "compiled_ibdoc", "compiled_assetcatalog",
                 "codesign.embedded_provisioningprofile"]

        // Make sure the inputs of this rule are only those rules which produce outputs compatible
        // with the type of the bundle being produced.
        excludedInputs: Bundle.excludedAuxiliaryInputs(project, product)

        outputFileTags: [
            "bundle.content",
            "bundle.symlink.headers", "bundle.symlink.private-headers",
            "bundle.symlink.resources", "bundle.symlink.executable",
            "bundle.symlink.version", "bundle.hpp", "bundle.resource",
            "bundle.provisioningprofile", "bundle.content.copied", "bundle.application-executable",
            "bundle.code-signature"]
        outputArtifacts: Bundle.generateBundleOutputs(product, inputs)
        prepare: Bundle.generateBundleCommands.apply(Bundle, arguments)
    }
}
