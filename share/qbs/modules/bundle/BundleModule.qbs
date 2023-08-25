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
        outputArtifacts: {
            var artifacts = [];
            var embed = product.bundle.embedInfoPlist;
            if (product.bundle.isBundle || embed) {
                artifacts.push({
                    filePath: FileInfo.joinPaths(
                                  product.destinationDirectory, product.name + "-Info.plist"),
                    fileTags: ["aggregate_infoplist"].concat(!embed ? ["bundle.input"] : []),
                    bundle: {
                        _bundleFilePath: FileInfo.joinPaths(
                                             product.destinationDirectory,
                                             product.bundle.infoPlistPath),
                    }
                });
            }
            return artifacts;
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating Info.plist for " + product.name;
            cmd.highlight = "codegen";
            cmd.infoPlist = product.bundle.infoPlist || {};
            cmd.processInfoPlist = product.bundle.processInfoPlist;
            cmd.infoPlistFormat = product.bundle.infoPlistFormat;
            cmd.extraEnv = product.bundle.extraEnv;
            cmd.qmakeEnv = product.bundle.qmakeEnv;

            // TODO: bundle module should know nothing about cpp module
            cmd.buildEnv = product.moduleProperty("cpp", "buildEnv");

            cmd.developerPath = product.bundle._developerPath;
            cmd.platformInfoPlist = product.bundle._platformInfoPlist;
            cmd.sdkSettingsPlist = product.bundle._sdkSettingsPlist;
            cmd.toolchainInfoPlist = product.bundle._toolchainInfoPlist;

            cmd.osBuildVersion = product.qbs.hostOSBuildVersion;

            cmd.sourceCode = function() {
                var plist, process, key, i;

                // Contains the combination of default, file, and in-source keys and values
                // Start out with the contents of this file as the "base", if given
                var aggregatePlist = {};

                for (i = 0; i < (inputs.infoplist || []).length; ++i) {
                    aggregatePlist =
                            BundleTools.infoPlistContents(inputs.infoplist[i].filePath);
                    infoPlistFormat = (infoPlistFormat === "same-as-input")
                            ? BundleTools.infoPlistFormat(inputs.infoplist[i].filePath)
                            : "xml1";
                    break;
                }

                // Add local key-value pairs (overrides equivalent keys specified in the file if
                // one was given)
                for (key in infoPlist) {
                    if (infoPlist.hasOwnProperty(key))
                        aggregatePlist[key] = infoPlist[key];
                }

                // Do some postprocessing if desired
                if (processInfoPlist) {
                    // Add default values to the aggregate plist if the corresponding keys
                    // for those values are not already present
                    var defaultValues = product.bundle.defaultInfoPlist;
                    for (key in defaultValues) {
                        if (defaultValues.hasOwnProperty(key) && !(key in aggregatePlist))
                            aggregatePlist[key] = defaultValues[key];
                    }

                    // Add keys from platform's Info.plist if not already present
                    var platformInfo = {};
                    var sdkSettings = {};
                    var toolchainInfo = {};
                    if (developerPath) {
                        plist = new PropertyList();
                        try {
                            plist.readFromFile(platformInfoPlist);
                            platformInfo = plist.toObject();
                        } finally {
                            plist.clear();
                        }

                        var additionalProps = platformInfo["AdditionalInfo"];
                        for (key in additionalProps) {
                            // override infoPlist?
                            if (additionalProps.hasOwnProperty(key) && !(key in aggregatePlist))
                                aggregatePlist[key] = defaultValues[key];
                        }
                        props = platformInfo['OverrideProperties'];
                        for (key in props) {
                            aggregatePlist[key] = props[key];
                        }

                        plist = new PropertyList();
                        try {
                            plist.readFromFile(sdkSettingsPlist);
                            sdkSettings = plist.toObject();
                        } finally {
                            plist.clear();
                        }

                        plist = new PropertyList();
                        try {
                            plist.readFromFile(toolchainInfoPlist);
                            toolchainInfo = plist.toObject();
                        } finally {
                            plist.clear();
                        }
                    }

                    aggregatePlist["BuildMachineOSBuild"] = osBuildVersion;

                    // setup env
                    env = {
                        "SDK_NAME": sdkSettings["CanonicalName"],
                        "XCODE_VERSION_ACTUAL": toolchainInfo["DTXcode"],
                        "SDK_PRODUCT_BUILD_VERSION": toolchainInfo["DTPlatformBuild"],
                        "GCC_VERSION": platformInfo["DTCompiler"],
                        "XCODE_PRODUCT_BUILD_VERSION": platformInfo["DTPlatformBuild"],
                        "PLATFORM_PRODUCT_BUILD_VERSION": platformInfo["ProductBuildVersion"],
                    }
                    env["MAC_OS_X_PRODUCT_BUILD_VERSION"] = osBuildVersion;

                    for (key in extraEnv)
                        env[key] = extraEnv[key];

                    for (key in buildEnv)
                        env[key] = buildEnv[key];

                    for (key in qmakeEnv)
                        env[key] = qmakeEnv[key];

                    var expander = new DarwinTools.PropertyListVariableExpander();
                    expander.undefinedVariableFunction = function (key, varName) {
                        var msg = "Info.plist variable expansion encountered undefined variable '"
                                + varName + "' when expanding value for key '" + key
                                + "', defined in one of the following files:\n\t";
                        var allFilePaths = [];

                        for (i = 0; i < (inputs.infoplist || []).length; ++i)
                            allFilePaths.push(inputs.infoplist[i].filePath);

                        if (platformInfoPlist)
                            allFilePaths.push(platformInfoPlist);
                        msg += allFilePaths.join("\n\t") + "\n";
                        msg += "or in the bundle.infoPlist property of product '"
                                + product.name + "'";
                        console.warn(msg);
                    };
                    aggregatePlist = expander.expand(aggregatePlist, env);

                    // Add keys from partial Info.plists from asset catalogs, XIBs, and storyboards.
                    for (var j = 0; j < (inputs.partial_infoplist || []).length; ++j) {
                        var partialInfoPlist =
                                BundleTools.infoPlistContents(
                                    inputs.partial_infoplist[j].filePath)
                                || {};
                        for (key in partialInfoPlist) {
                            if (partialInfoPlist.hasOwnProperty(key)
                                    && aggregatePlist[key] === undefined)
                                aggregatePlist[key] = partialInfoPlist[key];
                        }
                    }

                }

                // Anything with an undefined or otherwise empty value should be removed
                // Only JSON-formatted plists can have null values, other formats error out
                // This also follows Xcode behavior
                DarwinTools.cleanPropertyList(aggregatePlist);

                if (infoPlistFormat === "same-as-input")
                    infoPlistFormat = "xml1";

                var validFormats = [ "xml1", "binary1", "json" ];
                if (!validFormats.includes(infoPlistFormat))
                    throw("Invalid Info.plist format " + infoPlistFormat + ". " +
                          "Must be in [xml1, binary1, json].");

                // Write the plist contents in the format appropriate for the current platform
                plist = new PropertyList();
                try {
                    plist.readFromObject(aggregatePlist);
                    plist.writeToFile(outputs.aggregate_infoplist[0].filePath, infoPlistFormat);
                } finally {
                    plist.clear();
                }
            }
            return cmd;
        }
    }

    Rule {
        condition: qbs.targetOS.includes("darwin")
        multiplex: true
        inputs: ["aggregate_infoplist"]

        outputFileTags: ["bundle.input", "pkginfo"]
        outputArtifacts: {
            var artifacts = [];
            if (product.bundle.isBundle && product.bundle.generatePackageInfo) {
                artifacts.push({
                    filePath: FileInfo.joinPaths(product.destinationDirectory, "PkgInfo"),
                    fileTags: ["bundle.input", "pkginfo"],
                    bundle: { _bundleFilePath: FileInfo.joinPaths(product.destinationDirectory, product.bundle.pkgInfoPath) }
                });
            }
            return artifacts;
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating PkgInfo for " + product.name;
            cmd.highlight = "codegen";
            cmd.sourceCode = function() {
                var infoPlist = BundleTools.infoPlistContents(inputs.aggregate_infoplist[0].filePath);

                var pkgType = infoPlist['CFBundlePackageType'];
                if (!pkgType)
                    throw("CFBundlePackageType not found in Info.plist; this should not happen");

                var pkgSign = infoPlist['CFBundleSignature'];
                if (!pkgSign)
                    throw("CFBundleSignature not found in Info.plist; this should not happen");

                var pkginfo = new TextFile(outputs.pkginfo[0].filePath, TextFile.WriteOnly);
                pkginfo.write(pkgType + pkgSign);
                pkginfo.close();
            }
            return cmd;
        }
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
        outputArtifacts: {
            var i, artifacts = [];
            if (product.bundle.isBundle) {
                for (i in inputs["bundle.input"]) {
                    var fp = inputs["bundle.input"][i].bundle._bundleFilePath;
                    if (!fp)
                        throw("Artifact " + inputs["bundle.input"][i].filePath + " has no associated bundle file path");
                    var extraTags = inputs["bundle.input"][i].fileTags.includes("application")
                            ? ["bundle.application-executable"] : [];
                    artifacts.push({
                        filePath: fp,
                        fileTags: ["bundle.content", "bundle.content.copied"].concat(extraTags)
                    });
                }

                var provprofiles = inputs["codesign.embedded_provisioningprofile"];
                for (i in provprofiles) {
                    artifacts.push({
                        filePath: FileInfo.joinPaths(product.destinationDirectory,
                                                     product.bundle.contentsFolderPath,
                                                     provprofiles[i].fileName),
                        fileTags: ["bundle.provisioningprofile", "bundle.content"]
                    });
                }

                var packageType = product.bundle.packageType;
                var isShallow = product.bundle.isShallow;
                if (packageType === "FMWK" && !isShallow) {
                    var publicHeaders = product.bundle.publicHeaders;
                    if (publicHeaders && publicHeaders.length) {
                        artifacts.push({
                            filePath: FileInfo.joinPaths(product.destinationDirectory, product.bundle.bundleName, "Headers"),
                            fileTags: ["bundle.symlink.headers", "bundle.content"]
                        });
                    }

                    var privateHeaders = ModUtils.moduleProperty(product, "privateHeaders");
                    if (privateHeaders && privateHeaders.length) {
                        artifacts.push({
                            filePath: FileInfo.joinPaths(product.destinationDirectory, product.bundle.bundleName, "PrivateHeaders"),
                            fileTags: ["bundle.symlink.private-headers", "bundle.content"]
                        });
                    }

                    artifacts.push({
                        filePath: FileInfo.joinPaths(product.destinationDirectory, product.bundle.bundleName, "Resources"),
                        fileTags: ["bundle.symlink.resources", "bundle.content"]
                    });

                    artifacts.push({
                        filePath: FileInfo.joinPaths(product.destinationDirectory, product.bundle.bundleName, product.targetName),
                        fileTags: ["bundle.symlink.executable", "bundle.content"]
                    });

                    artifacts.push({
                        filePath: FileInfo.joinPaths(product.destinationDirectory, product.bundle.versionsFolderPath, "Current"),
                        fileTags: ["bundle.symlink.version", "bundle.content"]
                    });
                }

                var headerTypes = ["public", "private"];
                for (var h in headerTypes) {
                    var sources = ModUtils.moduleProperty(product, headerTypes[h] + "Headers");
                    var destination = FileInfo.joinPaths(product.destinationDirectory, ModUtils.moduleProperty(product, headerTypes[h] + "HeadersFolderPath"));
                    for (i in sources) {
                        artifacts.push({
                            filePath: FileInfo.joinPaths(destination, FileInfo.fileName(sources[i])),
                            fileTags: ["bundle.hpp", "bundle.content"]
                        });
                    }
                }

                sources = product.bundle.resources;
                for (i in sources) {
                    destination = BundleTools.destinationDirectoryForResource(product, {baseDir: FileInfo.path(sources[i]), fileName: FileInfo.fileName(sources[i])});
                    artifacts.push({
                        filePath: FileInfo.joinPaths(destination, FileInfo.fileName(sources[i])),
                        fileTags: ["bundle.resource", "bundle.content"]
                    });
                }

                var wrapperPath = FileInfo.joinPaths(
                            product.destinationDirectory,
                            product.bundle.bundleName);
                for (var i = 0; i < artifacts.length; ++i)
                    artifacts[i].bundle = { wrapperPath: wrapperPath };

                if (Host.os().includes("darwin") && product.codesign
                        && product.codesign.enableCodeSigning) {
                    artifacts.push({
                        filePath: FileInfo.joinPaths(product.bundle.contentsFolderPath, "_CodeSignature/CodeResources"),
                        fileTags: ["bundle.code-signature", "bundle.content"]
                    });
                }
            }
            return artifacts;
        }

        prepare: {
            var i, cmd, commands = [];
            var packageType = product.bundle.packageType;

            var bundleType = "bundle";
            if (packageType === "APPL")
                bundleType = "application";
            if (packageType === "FMWK")
                bundleType = "framework";

            // Product is unbundled
            if (!product.bundle.isBundle) {
                cmd = new JavaScriptCommand();
                cmd.silent = true;
                cmd.sourceCode = function () { };
                commands.push(cmd);
            }

            var symlinks = outputs["bundle.symlink.version"];
            for (i in symlinks) {
                cmd = new Command("ln", ["-sfn", product.bundle.frameworkVersion,
                                  symlinks[i].filePath]);
                cmd.silent = true;
                commands.push(cmd);
            }

            var publicHeaders = outputs["bundle.symlink.headers"];
            for (i in publicHeaders) {
                cmd = new Command("ln", ["-sfn", "Versions/Current/Headers",
                                         publicHeaders[i].filePath]);
                cmd.silent = true;
                commands.push(cmd);
            }

            var privateHeaders = outputs["bundle.symlink.private-headers"];
            for (i in privateHeaders) {
                cmd = new Command("ln", ["-sfn", "Versions/Current/PrivateHeaders",
                                         privateHeaders[i].filePath]);
                cmd.silent = true;
                commands.push(cmd);
            }

            var resources = outputs["bundle.symlink.resources"];
            for (i in resources) {
                cmd = new Command("ln", ["-sfn", "Versions/Current/Resources",
                                         resources[i].filePath]);
                cmd.silent = true;
                commands.push(cmd);
            }

            var executables = outputs["bundle.symlink.executable"];
            for (i in executables) {
                cmd = new Command("ln", ["-sfn", FileInfo.joinPaths("Versions", "Current", product.targetName),
                                         executables[i].filePath]);
                cmd.silent = true;
                commands.push(cmd);
            }

            function sortedArtifactList(list, func) {
                if (list) {
                    return list.sort(func || (function (a, b) {
                        return a.filePath.localeCompare(b.filePath);
                    }));
                }
            }

            var bundleInputs = sortedArtifactList(inputs["bundle.input"], function (a, b) {
                return a.bundle._bundleFilePath.localeCompare(
                            b.bundle._bundleFilePath);
            });
            var bundleContents = sortedArtifactList(outputs["bundle.content.copied"]);
            for (i in bundleContents) {
                cmd = new JavaScriptCommand();
                cmd.silent = true;
                cmd.source = bundleInputs[i].filePath;
                cmd.destination = bundleContents[i].filePath;
                cmd.sourceCode = function() {
                    File.copy(source, destination);
                };
                commands.push(cmd);
            }

            cmd = new JavaScriptCommand();
            cmd.description = "copying provisioning profile";
            cmd.highlight = "filegen";
            cmd.sources = (inputs["codesign.embedded_provisioningprofile"] || [])
                .map(function(artifact) { return artifact.filePath; });
            cmd.destination = (outputs["bundle.provisioningprofile"] || [])
                .map(function(artifact) { return artifact.filePath; });
            cmd.sourceCode = function() {
                var i;
                for (var i in sources) {
                    File.copy(sources[i], destination[i]);
                }
            };
            if (cmd.sources && cmd.sources.length)
                commands.push(cmd);

            cmd = new JavaScriptCommand();
            cmd.description = "copying public headers";
            cmd.highlight = "filegen";
            cmd.sources = product.bundle.publicHeaders;
            cmd.destination = FileInfo.joinPaths(product.destinationDirectory, product.bundle.publicHeadersFolderPath);
            cmd.sourceCode = function() {
                var i;
                for (var i in sources) {
                    File.copy(sources[i], FileInfo.joinPaths(destination, FileInfo.fileName(sources[i])));
                }
            };
            if (cmd.sources && cmd.sources.length)
                commands.push(cmd);

            cmd = new JavaScriptCommand();
            cmd.description = "copying private headers";
            cmd.highlight = "filegen";
            cmd.sources = product.bundle.privateHeaders;
            cmd.destination = FileInfo.joinPaths(product.destinationDirectory, product.bundle.privateHeadersFolderPath);
            cmd.sourceCode = function() {
                var i;
                for (var i in sources) {
                    File.copy(sources[i], FileInfo.joinPaths(destination, FileInfo.fileName(sources[i])));
                }
            };
            if (cmd.sources && cmd.sources.length)
                commands.push(cmd);

            cmd = new JavaScriptCommand();
            cmd.description = "copying resources";
            cmd.highlight = "filegen";
            cmd.sources = product.bundle.resources;
            cmd.sourceCode = function() {
                var i;
                for (var i in sources) {
                    var destination = BundleTools.destinationDirectoryForResource(product, {baseDir: FileInfo.path(sources[i]), fileName: FileInfo.fileName(sources[i])});
                    File.copy(sources[i], FileInfo.joinPaths(destination, FileInfo.fileName(sources[i])));
                }
            };
            if (cmd.sources && cmd.sources.length)
                commands.push(cmd);

            if (product.qbs.hostOS.includes("darwin")) {
                Array.prototype.push.apply(commands, Codesign.prepareSign(
                                               project, product, inputs, outputs, input, output));

                if (bundleType === "application"
                        && product.qbs.targetOS.includes("macos")) {
                    var bundlePath = FileInfo.joinPaths(
                        product.destinationDirectory, product.bundle.bundleName);
                    cmd = new Command(product.bundle.lsregisterPath,
                                      ["-f", bundlePath]);
                    cmd.description = "registering " + product.bundle.bundleName;
                    commands.push(cmd);
                }
            }

            return commands;
        }
    }
}
