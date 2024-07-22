/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

var Codesign = require("../codesign/codesign.js");
var File = require("qbs.File");
var FileInfo = require("qbs.FileInfo");
var DarwinTools = require("qbs.DarwinTools");
var ModUtils = require("qbs.ModUtils");
var Process = require("qbs.Process");
var Utilities = require("qbs.Utilities");

// HACK: Workaround until the PropertyList extension is supported cross-platform
var TextFile = require("qbs.TextFile");
var PropertyList2 = (function () {
    function PropertyList2() {
    }
    PropertyList2.prototype.readFromFile = function (filePath) {
        var str;
        var process = new Process();
        try {
            if (process.exec("plutil", ["-convert", "json", "-o", "-", filePath], false) === 0) {
                str = process.readStdOut();
            } else {
                var tf = new TextFile(filePath);
                try {
                    str = tf.readAll();
                } finally {
                    tf.close();
                }
            }
        } finally {
            process.close();
        }

        if (str)
            this.obj = JSON.parse(str);
    };
    PropertyList2.prototype.toObject = function () {
        return this.obj;
    };
    PropertyList2.prototype.clear = function () {
    };
    return PropertyList2;
}());

// Order is significant due to productTypeIdentifier() search path
var _productTypeIdentifiers = {
    "inapppurchase": "com.apple.product-type.in-app-purchase-content",
    "applicationextension": "com.apple.product-type.app-extension",
    "xpcservice": "com.apple.product-type.xpc-service",
    "application": "com.apple.product-type.application",
    "dynamiclibrary": "com.apple.product-type.framework",
    "loadablemodule": "com.apple.product-type.bundle",
    "staticlibrary": "com.apple.product-type.framework.static",
    "kernelmodule": "com.apple.product-type.kernel-extension"
};

function productTypeIdentifier(productType) {
    for (var k in _productTypeIdentifiers) {
        if (productType.includes(k))
            return _productTypeIdentifiers[k];
    }
    return "com.apple.package-type.wrapper";
}

function excludedAuxiliaryInputs(project, product) {
    var chain = product.bundle._productTypeIdentifierChain;
    var bestPossibleType;
    for (var i = chain.length - 1; i >= 0; --i) {
        switch (chain[i]) {
        case "com.apple.product-type.bundle":
            bestPossibleType = "loadablemodule";
            break;
        case "com.apple.product-type.framework":
            bestPossibleType = "dynamiclibrary";
            break;
        case "com.apple.product-type.framework.static":
            bestPossibleType = "staticlibrary";
            break;
        case "com.apple.product-type.application":
        case "com.apple.product-type.xpc-service":
            bestPossibleType = "application";
            break;
        }
    }

    var excluded = [];
    var possibleTypes = ["application", "dynamiclibrary", "staticlibrary", "loadablemodule"];
    for (i = 0; i < possibleTypes.length; ++i) {
        if (possibleTypes[i] !== bestPossibleType)
            excluded.push(possibleTypes[i]);
    }

    return excluded;
}

function packageType(productTypeIdentifier) {
    switch (productTypeIdentifier) {
    case "com.apple.product-type.in-app-purchase-content":
        return undefined;
    case "com.apple.product-type.app-extension":
    case "com.apple.product-type.xpc-service":
        return "XPC!";
    case "com.apple.product-type.application":
        return "APPL";
    case "com.apple.product-type.framework":
    case "com.apple.product-type.framework.static":
        return "FMWK";
    case "com.apple.product-type.kernel-extension":
    case "com.apple.product-type.kernel-extension.iokit":
        return "KEXT";
    default:
        return "BNDL";
    }
}

function _assign(target, source) {
    if (source) {
        for (var k in source) {
            if (source.hasOwnProperty(k))
                target[k] = source[k];
        }
        return target;
    }
}

function macOSSpecsPaths(version, developerPath) {
    var result = [];
    if (Utilities.versionCompare(version, "15.3") >= 0) {
        result.push(FileInfo.joinPaths(
            developerPath, "..", "SharedFrameworks", "XCBuild.framework", "PlugIns",
            "XCBBuildService.bundle", "Contents", "PlugIns", "XCBSpecifications.ideplugin",
            "Contents", "Resources"));
    }
    if (Utilities.versionCompare(version, "14.3") >= 0) {
        result.push(FileInfo.joinPaths(
                    developerPath, "Library", "Xcode", "Plug-ins", "XCBSpecifications.ideplugin",
                    "Contents", "Resources"));
    } else if (Utilities.versionCompare(version, "12.5") >= 0) {
        result.push(FileInfo.joinPaths(
                    developerPath, "..", "PlugIns", "XCBSpecifications.ideplugin",
                    "Contents", "Resources"));
    } else if (Utilities.versionCompare(version, "12") >= 0) {
        result.push(FileInfo.joinPaths(
                    developerPath, "Platforms", "MacOSX.platform", "Developer", "Library", "Xcode",
                    "PrivatePlugIns", "IDEOSXSupportCore.ideplugin", "Contents", "Resources"));
    } else {
        result.push(FileInfo.joinPaths(
                    developerPath, "Platforms", "MacOSX.platform", "Developer", "Library", "Xcode",
                    "Specifications"));
    }
    return result;
}

var XcodeBuildSpecsReader = (function () {
    function XcodeBuildSpecsReader(specsPaths, separator, additionalSettings, useShallowBundles) {
        this._additionalSettings = additionalSettings;
        this._useShallowBundles = useShallowBundles;

        this._packageTypes = [];
        this._productTypes = [];

        var i, j;
        for (i = 0; i < specsPaths.length; ++i) {
            var specsPath = specsPaths[i];
            var names = ["", "Darwin", "MacOSX"];
            for (j = 0; j < names.length; ++j) {
                var name = names[j];
                var plist = new PropertyList2();
                var plist2 = new PropertyList2();
                try
                {
                    var plistName = [name, "Package", "Types.xcspec"].join(name ? separator : "");
                    var plistName2 = [name, "Product", "Types.xcspec"].join(name ? separator : "");
                    var plistPath = FileInfo.joinPaths(specsPath, plistName);
                    var plistPath2 = FileInfo.joinPaths(specsPath, plistName2);
                    if (File.exists(plistPath)) {
                        plist.readFromFile(plistPath);
                        this._packageTypes = this._packageTypes.concat(plist.toObject());
                    }
                    if (File.exists(plistPath2)) {
                        plist2.readFromFile(plistPath2);
                        this._productTypes = this._productTypes.concat(plist2.toObject());
                    }
                } finally {
                    plist.clear();
                    plist2.clear();
                }
            }
        }

        this._types = {};
        for (i = 0; i < this._packageTypes.length; ++i)
            this._types[this._packageTypes[i]["Identifier"]] = this._packageTypes[i];
        for (i = 0; i < this._productTypes.length; ++i)
            this._types[this._productTypes[i]["Identifier"]] = this._productTypes[i];

    }
    XcodeBuildSpecsReader.prototype.productTypeIdentifierChain = function (typeIdentifier) {
        var ids = [typeIdentifier];
        var obj = this._types[typeIdentifier];
        var parentId = obj && obj["BasedOn"];
        if (parentId)
            return ids.concat(this.productTypeIdentifierChain(parentId));
        return ids;
    };
    XcodeBuildSpecsReader.prototype.settings = function (typeIdentifier, recursive, skipPackageTypes) {
        // Silently use shallow bundles when preferred since it seems to be some sort of automatic
        // shadowing mechanism. For example, this matches Xcode behavior where static frameworks
        // are shallow even though no such product specification exists, and also seems to match
        // other behavior i.e. where productType in pbxproj files is never explicitly shallow.
        if (this._useShallowBundles && this._types[typeIdentifier + ".shallow"] && !skipPackageTypes)
            typeIdentifier += ".shallow";

        var typesObject = this._types[typeIdentifier];
        if (typesObject) {
            var buildProperties = {};

            if (recursive) {
                // Get all the settings for the product's package type
                if (!skipPackageTypes && typesObject["PackageTypes"]) {
                    for (var k = 0; k < typesObject["PackageTypes"].length; ++k) {
                        var props = this.settings(typesObject["PackageTypes"][k], recursive, true);
                        for (var y in props) {
                            if (props.hasOwnProperty(y))
                                buildProperties[y] = props[y];
                        }
                        break;
                    }
                }

                // Get all the settings for the product's inherited product type
                if (typesObject["BasedOn"]) {
                    // We'll only do the auto shallow substitution for wrapper package types...
                    // this ensures that in-app purchase content bundles are non-shallow on both
                    // macOS and iOS, for example (which matches Xcode behavior)
                    var isWrapper = false;
                    if (typesObject["ProductReference"]) {
                        var fileType = typesObject["ProductReference"]["FileType"];
                        if (fileType)
                            isWrapper = fileType.startsWith("wrapper.");
                    }

                    // Prevent recursion loop if this spec's base plus .shallow would be the same
                    // as the current spec's identifier
                    var baseIdentifier = typesObject["BasedOn"];
                    if (this._useShallowBundles && isWrapper
                            && this._types[baseIdentifier + ".shallow"]
                            && typeIdentifier !== baseIdentifier + ".shallow")
                        baseIdentifier += ".shallow";

                    props = this.settings(baseIdentifier, recursive, true);
                    for (y in props) {
                        if (props.hasOwnProperty(y))
                            buildProperties[y] = props[y];
                    }
                }
            }


            if (typesObject["Type"] === "PackageType") {
                props = typesObject["DefaultBuildSettings"];
                for (y in props) {
                    if (props.hasOwnProperty(y))
                        buildProperties[y] = props[y];
                }
            }

            if (typesObject["Type"] === "ProductType") {
                props = typesObject["DefaultBuildProperties"];
                for (y in props) {
                    if (props.hasOwnProperty(y))
                        buildProperties[y] = props[y];
                }
            }

            return buildProperties;
        }
    };
    XcodeBuildSpecsReader.prototype.setting = function (typeIdentifier, settingName) {
        var obj = this.settings(typeIdentifier, false);
        if (obj) {
            return obj[settingName];
        }
    };
    XcodeBuildSpecsReader.prototype.expandedSettings = function (typeIdentifier, baseSettings) {
        var obj = this.settings(typeIdentifier, true);
        if (obj) {
            for (var k in obj)
                obj[k] = this.expandedSetting(typeIdentifier, baseSettings, k);
            return obj;
        }
    };
    XcodeBuildSpecsReader.prototype.expandedSetting = function (typeIdentifier, baseSettings,
                                                                settingName) {
        var obj = {};
        _assign(obj, baseSettings); // todo: copy recursively
        obj = _assign(obj, this.settings(typeIdentifier, true));
        if (obj) {
            for (var x in this._additionalSettings) {
                var additionalSetting = this._additionalSettings[x];
                if (additionalSetting !== undefined)
                    obj[x] = additionalSetting;
            }
            var setting = obj[settingName];
            var original;
            while (original !== setting) {
                original = setting;
                setting = DarwinTools.expandPlistEnvironmentVariables({ key: setting }, obj, false)["key"];
            }
            return setting;
        }
    };
    return XcodeBuildSpecsReader;
}());

function generateBundleOutputs(product, inputs)
{
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

function generateBundleCommands(project, product, inputs, outputs, input, output,
                                explicitlyDependsOn)
{
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

function generatePkgInfoOutputs(product)
{
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

function generatePkgInfoCommands(project, product, inputs, outputs, input, output,
                                 explicitlyDependsOn)
{
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

function aggregateInfoPlistOutputs(product)
{
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

function aggregateInfoPlistCommands(project, product, inputs, outputs, input, output,
                                    explicitlyDependsOn)
{
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
