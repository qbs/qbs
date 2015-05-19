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
import qbs.BundleTools
import qbs.DarwinTools
import qbs.File
import qbs.FileInfo
import qbs.ModUtils
import qbs.PropertyList
import qbs.TextFile

Module {
    additionalProductTypes: ["bundle"]

    property bool isBundle: qbs.targetOS.contains("darwin")
                            && (product.type.contains("application")
                                || product.type.contains("dynamiclibrary")
                                || product.type.contains("loadablemodule"))
                            && !product.consoleApplication
    property bool isShallow: qbs.targetOS.contains("ios") && product.type.contains("application")

    property string identifierPrefix: "org.example"
    property string identifier: [identifierPrefix, qbs.rfc1034Identifier(product.targetName)].join(".")

    property string extension: {
        if (packageType === undefined) {
            return "";
        } else if (packageType === "APPL") {
            return "app";
        } else if (packageType === "XPC!") {
            if (product.type.contains("applicationextension"))
                return "appex";
            return "xpc";
        } else if (packageType === "FMWK") {
            return "framework";
        } else{
            return "bundle";
        }

        // Also: kext, prefPane, qlgenerator, saver, mdimporter, or a custom extension
    }

    property string packageType: {
        if (product.type.contains("inapppurchase"))
            return undefined;
        if (product.type.contains("xpcservice"))
            return "XPC!";
        if (product.type.contains("application"))
            return "APPL";
        if (product.type.contains("dynamiclibrary") || product.type.contains("staticlibrary"))
            return "FMWK";
        return "BNDL";
    }

    property string signature: "????" // legacy creator code in Mac OS Classic (CFBundleSignature), can be ignored

    property string bundleName: product.targetName + (extension ? ("." + extension) : "")

    property string frameworkVersion: {
        if (packageType === "FMWK") {
            var n = parseInt(product.version, 10);
            return isNaN(n) ? 'A' : String(n);
        }
    }

    property pathList publicHeaders
    property pathList privateHeaders
    property pathList resources

    property path infoPlistFile
    property var infoPlist
    property bool processInfoPlist: true
    property bool embedInfoPlist: product.type.contains("application") && !isBundle
    property string infoPlistFormat: {
        if (qbs.targetOS.contains("osx"))
            return infoPlistFile ? "same-as-input" : "xml1";
        else if (qbs.targetOS.contains("ios"))
            return "binary1";
    }

    property string localizedResourcesFolderSuffix: ".lproj"

    property string lsregisterName: "lsregister"
    property string lsregisterPath: FileInfo.joinPaths(
                                        "/System/Library/Frameworks/CoreServices.framework" +
                                        "/Versions/A/Frameworks/LaunchServices.framework" +
                                        "/Versions/A/Support", lsregisterName);

    // all paths are relative to the directory containing the bundle
    readonly property string infoPlistPath: {
        var path;
        if (!isBundle)
            path = FileInfo.joinPaths(".tmp", product.name);
        else if (packageType === "FMWK")
            path = unlocalizedResourcesFolderPath;
        else if (product.type.contains("inapppurchase"))
            path = bundleName;
        else
            path = contentsFolderPath;

        return FileInfo.joinPaths(path, product.type.contains("inapppurchase") ? "ContentInfo.plist" : "Info.plist");
    }

    readonly property string pkgInfoPath: FileInfo.joinPaths(packageType === "FMWK" ? bundleName : contentsFolderPath, "PkgInfo")
    readonly property string versionPlistPath: FileInfo.joinPaths(packageType === "FMWK" ? unlocalizedResourcesFolderPath : contentsFolderPath, "version.plist")

    readonly property string executablePath: FileInfo.joinPaths(executableFolderPath, product.targetName)

    readonly property string executableFolderPath: (!isShallow && packageType !== "FMWK" && !isShallowContents) ? FileInfo.joinPaths(contentsFolderPath, "MacOS") : contentsFolderPath
    readonly property string executablesFolderPath: packageType === "FMWK" ? unlocalizedResourcesFolderPath : FileInfo.joinPaths(contentsFolderPath, !isShallowContents ? "Executables" : "")
    readonly property string frameworksFolderPath: FileInfo.joinPaths(contentsFolderPath, !isShallowContents ? "Frameworks" : "")
    readonly property string pluginsFolderPath: packageType === "FMWK" ? unlocalizedResourcesFolderPath : FileInfo.joinPaths(contentsFolderPath, !isShallowContents ? "PlugIns" : "")
    readonly property string privateHeadersFolderPath: FileInfo.joinPaths(contentsFolderPath, !isShallowContents ? "PrivateHeaders" : "")
    readonly property string publicHeadersFolderPath: FileInfo.joinPaths(contentsFolderPath, !isShallowContents ? "Headers" : "")
    readonly property string scriptsFolderPath: FileInfo.joinPaths(unlocalizedResourcesFolderPath, "Scripts")
    readonly property string sharedFrameworksFolderPath: FileInfo.joinPaths(contentsFolderPath, !isShallowContents ? "SharedFrameworks" : "")
    readonly property string sharedSupportFolderPath: packageType === "FMWK" ? unlocalizedResourcesFolderPath : FileInfo.joinPaths(contentsFolderPath, !isShallowContents ? "SharedSupport" : "")
    readonly property string unlocalizedResourcesFolderPath: isShallow ? contentsFolderPath : FileInfo.joinPaths(contentsFolderPath, !isShallowContents ? "Resources" : "")

    readonly property string contentsFolderPath: {
        if (packageType === "FMWK")
            return FileInfo.joinPaths(bundleName, "Versions", frameworkVersion);
        else if (!isShallow)
            return FileInfo.joinPaths(bundleName, "Contents");
        return bundleName;
    }

    // private properties
    readonly property bool isShallowContents: product.type.contains("inapppurchase")

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

    Rule {
        condition: qbs.targetOS.contains("darwin")
        multiplex: true
        inputs: ["qbs", "partial_infoplist"]

        outputFileTags: ["infoplist"]
        outputArtifacts: {
            var artifacts = [];
            if (ModUtils.moduleProperty(product, "isBundle") || ModUtils.moduleProperty(product, "embedInfoPlist")) {
                artifacts.push({
                    filePath: FileInfo.joinPaths(product.destinationDirectory, ModUtils.moduleProperty(product, "infoPlistPath")),
                    fileTags: ["infoplist"]
                });
            }
            return artifacts;
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating Info.plist for " + product.name;
            cmd.highlight = "codegen";
            cmd.partialInfoPlistFiles = inputs.partial_infoplist;
            cmd.infoPlistFile = ModUtils.moduleProperty(product, "infoPlistFile");
            cmd.infoPlist = ModUtils.moduleProperty(product, "infoPlist") || {};
            cmd.processInfoPlist = ModUtils.moduleProperty(product, "processInfoPlist");
            cmd.infoPlistFormat = ModUtils.moduleProperty(product, "infoPlistFormat");
            cmd.qmakeEnv = ModUtils.moduleProperty(product, "qmakeEnv");

            cmd.platformPath = product.moduleProperty("cpp", "platformPath");
            cmd.toolchainInstallPath = product.moduleProperty("cpp", "toolchainInstallPath");
            cmd.buildEnv = product.moduleProperty("cpp", "buildEnv");
            cmd.defines = product.moduleProperty("cpp", "defines");
            cmd.platformDefines = product.moduleProperty("cpp", "platformDefines");
            cmd.compilerDefines = product.moduleProperty("cpp", "compilerDefines");
            cmd.allDefines = [].concat(cmd.defines || []).concat(cmd.platformDefines || []).concat(cmd.compilerDefines || []);

            cmd.platformInfoPlist = product.moduleProperty("cpp", "platformInfoPlist");
            cmd.sdkSettingsPlist = product.moduleProperty("cpp", "sdkSettingsPlist");
            cmd.toolchainInfoPlist = product.moduleProperty("cpp", "toolchainInfoPlist");

            cmd.sysroot = product.moduleProperty("qbs", "sysroot");
            cmd.osBuildVersion = product.moduleProperty("qbs", "hostOSBuildVersion");

            cmd.sourceCode = function() {
                var plist, process, key, i;

                // Contains the combination of default, file, and in-source keys and values
                // Start out with the contents of this file as the "base", if given
                var aggregatePlist = BundleTools.infoPlistContents(infoPlistFile) || {};

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
                    var defaultValues = ModUtils.moduleProperty(product, "defaultInfoPlist");
                    for (key in defaultValues) {
                        if (defaultValues.hasOwnProperty(key) && !(key in aggregatePlist))
                            aggregatePlist[key] = defaultValues[key];
                    }

                    var defaultValues = product.moduleProperty("cpp", "defaultInfoPlist");
                    for (key in defaultValues) {
                        if (defaultValues.hasOwnProperty(key) && !(key in aggregatePlist))
                            aggregatePlist[key] = defaultValues[key];
                    }

                    // Add keys from platform's Info.plist if not already present
                    var platformInfo = {};
                    if (platformPath) {
                        if (File.exists(platformInfoPlist)) {
                            plist = new PropertyList();
                            try {
                                plist.readFromFile(platformInfoPlist);
                                platformInfo = plist.toObject();
                            } finally {
                                plist.clear();
                            }

                            var additionalProps = platformInfo["AdditionalInfo"];
                            for (key in additionalProps) {
                                if (additionalProps.hasOwnProperty(key) && !(key in aggregatePlist)) // override infoPlist?
                                    aggregatePlist[key] = defaultValues[key];
                            }
                            props = platformInfo['OverrideProperties'];
                            for (key in props) {
                                aggregatePlist[key] = props[key];
                            }

                            if (product.moduleProperty("qbs", "targetOS").contains("ios")) {
                                key = "UIDeviceFamily";
                                if (key in platformInfo && !(key in aggregatePlist))
                                    aggregatePlist[key] = platformInfo[key];
                            }
                        } else {
                            print("warning: platform path given but no platform Info.plist found");
                        }
                    } else {
                        print("no platform path specified");
                    }

                    var sdkSettings = {};
                    if (sysroot) {
                        if (File.exists(sdkSettingsPlist)) {
                            plist = new PropertyList();
                            try {
                                plist.readFromFile(sdkSettingsPlist);
                                sdkSettings = plist.toObject();
                            } finally {
                                plist.clear();
                            }
                        } else {
                            print("warning: sysroot (SDK path) given but no SDKSettings.plist found");
                        }
                    } else {
                        print("no sysroot (SDK path) specified");
                    }

                    var toolchainInfo = {};
                    if (toolchainInstallPath && File.exists(toolchainInfoPlist)) {
                        plist = new PropertyList();
                        try {
                            plist.readFromFile(toolchainInfoPlist);
                            toolchainInfo = plist.toObject();
                        } finally {
                            plist.clear();
                        }
                    } else {
                        print("could not find a ToolchainInfo.plist near the toolchain install path");
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

                    for (key in buildEnv)
                        env[key] = buildEnv[key];

                    for (key in qmakeEnv)
                        env[key] = qmakeEnv[key];

                    for (i = 0; i < allDefines.length; ++i) {
                        var parts = allDefines[i].split('=');
                        env[parts[0]] = parts[1];
                    }

                    DarwinTools.expandPlistEnvironmentVariables(aggregatePlist, env, true);

                    // Add keys from partial Info.plists from asset catalogs, XIBs, and storyboards
                    for (i in partialInfoPlistFiles) {
                        var partialInfoPlist = BundleTools.infoPlistContents(partialInfoPlistFiles[i].filePath) || {};
                        for (key in partialInfoPlist) {
                            if (partialInfoPlist.hasOwnProperty(key))
                                aggregatePlist[key] = partialInfoPlist[key];
                        }
                    }
                }

                // Anything with an undefined or otherwise empty value should be removed
                // Only JSON-formatted plists can have null values, other formats error out
                // This also follows Xcode behavior
                DarwinTools.cleanPropertyList(aggregatePlist);

                if (infoPlistFormat === "same-as-input" && infoPlistFile)
                    infoPlistFormat = BundleTools.infoPlistFormat(infoPlistFile);

                var validFormats = [ "xml1", "binary1", "json" ];
                if (!validFormats.contains(infoPlistFormat))
                    throw("Invalid Info.plist format " + infoPlistFormat + ". " +
                          "Must be in [xml1, binary1, json].");

                // Write the plist contents in the format appropriate for the current platform
                plist = new PropertyList();
                try {
                    plist.readFromObject(aggregatePlist);
                    plist.writeToFile(outputs.infoplist[0].filePath, infoPlistFormat);
                } finally {
                    plist.clear();
                }
            }
            return cmd;
        }
    }

    Rule {
        condition: qbs.targetOS.contains("darwin")
        multiplex: true
        inputs: ["infoplist"]

        outputFileTags: ["pkginfo"]
        outputArtifacts: {
            var artifacts = [];
            if (ModUtils.moduleProperty(product, "isBundle")) {
                artifacts.push({
                    filePath: FileInfo.joinPaths(product.destinationDirectory, ModUtils.moduleProperty(product, "pkgInfoPath")),
                    fileTags: ["pkginfo"]
                });
            }
            return artifacts;
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating PkgInfo for " + product.name;
            cmd.highlight = "codegen";
            cmd.sourceCode = function() {
                var infoPlist = BundleTools.infoPlistContents(inputs.infoplist[0].filePath);

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
        condition: qbs.targetOS.contains("darwin")
        multiplex: true
        inputs: ["infoplist", "pkginfo", "hpp",
                 "icns", "resourcerules", "ipa",
                 "compiled_nib", "compiled_storyboard", "compiled_assetcatalog"]

        outputFileTags: ["bundle",
            "bundle.symlink.headers", "bundle.symlink.private-headers",
            "bundle.symlink.resources", "bundle.symlink.executable",
            "bundle.symlink.version", "bundle.hpp", "bundle.resource"]
        outputArtifacts: {
            var i, artifacts = [];
            if (ModUtils.moduleProperty(product, "isBundle")) {
                artifacts.push({
                    filePath: FileInfo.joinPaths(product.destinationDirectory, ModUtils.moduleProperty(product, "bundleName")),
                    fileTags: ["bundle"]
                });

                var packageType = ModUtils.moduleProperty(product, "packageType");
                if (packageType === "FMWK") {
                    var publicHeaders = ModUtils.moduleProperties(product, "publicHeaders");
                    if (publicHeaders && publicHeaders.length) {
                        artifacts.push({
                            filePath: FileInfo.joinPaths(product.destinationDirectory, ModUtils.moduleProperty(product, "bundleName"), "Headers"),
                            fileTags: ["bundle.symlink.headers"]
                        });
                    }

                    var privateHeaders = ModUtils.moduleProperties(product, "privateHeaders");
                    if (privateHeaders && privateHeaders.length) {
                        artifacts.push({
                            filePath: FileInfo.joinPaths(product.destinationDirectory, ModUtils.moduleProperty(product, "bundleName"), "PrivateHeaders"),
                            fileTags: ["bundle.symlink.private-headers"]
                        });
                    }

                    artifacts.push({
                        filePath: FileInfo.joinPaths(product.destinationDirectory, ModUtils.moduleProperty(product, "bundleName"), "Resources"),
                        fileTags: ["bundle.symlink.resources"]
                    });

                    artifacts.push({
                        filePath: FileInfo.joinPaths(product.destinationDirectory, ModUtils.moduleProperty(product, "bundleName"), product.targetName),
                        fileTags: ["bundle.symlink.executable"]
                    });

                    artifacts.push({
                        filePath: FileInfo.joinPaths(product.destinationDirectory, ModUtils.moduleProperty(product, "bundleName"), "Versions", "Current"),
                        fileTags: ["bundle.symlink.version"]
                    });
                }

                var headerTypes = ["public", "private"];
                for (var h in headerTypes) {
                    var sources = ModUtils.moduleProperties(product, headerTypes[h] + "Headers");
                    var destination = FileInfo.joinPaths(product.destinationDirectory, ModUtils.moduleProperty(product, headerTypes[h] + "HeadersFolderPath"));
                    for (i in sources) {
                        artifacts.push({
                            filePath: FileInfo.joinPaths(destination, FileInfo.fileName(sources[i])),
                            fileTags: ["bundle.hpp"]
                        });
                    }
                }

                sources = ModUtils.moduleProperties(product, "resources");
                for (i in sources) {
                    destination = BundleTools.destinationDirectoryForResource(product, {baseDir: FileInfo.path(sources[i]), fileName: FileInfo.fileName(sources[i])});
                    artifacts.push({
                        filePath: FileInfo.joinPaths(destination, FileInfo.fileName(sources[i])),
                        fileTags: ["bundle.resource"]
                    });
                }
            }
            return artifacts;
        }

        prepare: {
            var i, cmd, commands = [];
            var packageType = ModUtils.moduleProperty(product, "packageType");

            var bundleType = "bundle";
            if (packageType === "APPL")
                bundleType = "application";
            if (packageType === "FMWK")
                bundleType = "framework";

            var bundles = outputs.bundle;
            for (i in bundles) {
                cmd = new Command("mkdir", ["-p", bundles[i].filePath]);
                cmd.description = "creating " + bundleType + " " + product.targetName;
                commands.push(cmd);

                cmd = new Command("touch", ["-c", bundles[i].filePath]);
                cmd.silent = true;
                commands.push(cmd);
            }

            // Product is unbundled
            if (commands.length === 0) {
                cmd = new JavaScriptCommand();
                cmd.silent = true;
                cmd.sourceCode = function () { };
                commands.push(cmd);
            }

            var symlinks = outputs["bundle.symlink.version"];
            for (i in symlinks) {
                cmd = new Command("ln", ["-sfn", ModUtils.moduleProperty(product, "frameworkVersion"),
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
                cmd = new Command("ln", ["-sf", FileInfo.joinPaths("Versions", "Current", product.targetName),
                                         executables[i].filePath]);
                cmd.silent = true;
                commands.push(cmd);
            }

            cmd = new JavaScriptCommand();
            cmd.description = "copying public headers";
            cmd.highlight = "filegen";
            cmd.sources = ModUtils.moduleProperties(product, "publicHeaders");
            cmd.destination = FileInfo.joinPaths(product.destinationDirectory, ModUtils.moduleProperty(product, "publicHeadersFolderPath"));
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
            cmd.sources = ModUtils.moduleProperties(product, "privateHeaders");
            cmd.destination = FileInfo.joinPaths(product.destinationDirectory, ModUtils.moduleProperty(product, "privateHeadersFolderPath"));
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
            cmd.sources = ModUtils.moduleProperties(product, "resources");
            cmd.sourceCode = function() {
                var i;
                for (var i in sources) {
                    var destination = BundleTools.destinationDirectoryForResource(product, {baseDir: FileInfo.path(sources[i]), fileName: FileInfo.fileName(sources[i])});
                    File.copy(sources[i], FileInfo.joinPaths(destination, FileInfo.fileName(sources[i])));
                }
            };
            if (cmd.sources && cmd.sources.length)
                commands.push(cmd);

            if (product.type.contains("application") && product.moduleProperty("qbs", "hostOS").contains("darwin")) {
                for (i in bundles) {
                    cmd = new Command(ModUtils.moduleProperty(product, "lsregisterPath"),
                                      ["-f", bundles[i].filePath]);
                    cmd.description = "register " + ModUtils.moduleProperty(product, "bundleName");
                    commands.push(cmd);
                }
            }

            return commands;
        }
    }
}
