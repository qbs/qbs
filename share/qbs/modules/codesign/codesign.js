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

var File = require("qbs.File");
var FileInfo = require("qbs.FileInfo");
var PathTools = require("qbs.PathTools");
var Process = require("qbs.Process");
var PropertyList = require("qbs.PropertyList");
var Utilities = require("qbs.Utilities");

function findSigningIdentities(searchString, team) {
    if (!searchString)
        return {};
    var identities = Utilities.signingIdentities();
    var matchedIdentities = {};
    for (var key in identities) {
        var identity = identities[key];
        if (team && ![identity.subjectInfo.O, identity.subjectInfo.OU].includes(team))
            continue;
        if (searchString === key
                || (identity.subjectInfo.CN && identity.subjectInfo.CN.startsWith(searchString))) {
            matchedIdentities[key] = identity;
        }
    }
    return matchedIdentities;
}

function humanReadableIdentitySummary(identities) {
    return "\n\t" + Object.keys(identities).map(function (key) {
        return identities[key].subjectInfo.CN
                + " in team "
                + identities[key].subjectInfo.O
                + " (" + identities[key].subjectInfo.OU + ")";
    }).join("\n\t");
}

/**
  * Returns the best provisioning profile for code signing a binary with the given parameters.
  * Ideally, this should behave identically as Xcode but the algorithm is not documented
  * \l{https://developer.apple.com/library/ios/qa/qa1814/_index.html}{Automatic Provisioning}
  */
function findBestProvisioningProfile(product, files) {
    var actualSigningIdentity = product.codesign._actualSigningIdentity || {};
    var teamIdentifier = product.codesign.teamIdentifier;
    var bundleIdentifier = product.bundle.identifier;
    var targetOS = product.qbs.targetOS;
    var buildVariant = product.qbs.buildVariant;
    var query = product.codesign.provisioningProfile;
    var profilePlatform = product.codesign._provisioningProfilePlatform;

    // Read all provisioning profiles on disk into plist objects in memory
    var profiles = files.map(function(filePath) {
        var plist = new PropertyList();
        try {
            plist.readFromData(Utilities.smimeMessageContent(filePath));
            return {
                data: plist.toObject(),
                filePath: filePath
            };
        } finally {
            plist.clear();
        }
    });

    // Do a simple search by matching UUID or Name
    if (query) {
        for (var i = 0; i < profiles.length; ++i) {
            var obj = profiles[i];
            if (obj.data && (obj.data.UUID === query || obj.data.Name === query))
                return obj;
        }

        // If we asked for a specific provisioning profile, don't select one automatically
        return undefined;
    }

    // Provisioning profiles are not normally used with ad-hoc code signing or non-apps
    // We do these checks down here only for the automatic selection but not above because
    // if the user explicitly selects a provisioning profile it should be used no matter what
    if (actualSigningIdentity.SHA1 === "-" || !product.type.includes("application"))
        return undefined;

    // Filter out any provisioning profiles we know to be unsuitable from the start
    profiles = profiles.filter(function (profile) {
        var data = profile.data;

        if (actualSigningIdentity.subjectInfo) {
            var certCommonNames = (data["DeveloperCertificates"] || []).map(function (cert) {
                return Utilities.certificateInfo(cert).subjectInfo.CN;
            });
            if (!certCommonNames.includes(actualSigningIdentity.subjectInfo.CN)) {
                console.log("Skipping provisioning profile with no matching certificate names for '"
                            + actualSigningIdentity.subjectInfo.CN
                            + "' (found " + certCommonNames.join(", ") + "): "
                            + profile.filePath);
                return false;
            }
        }

        var platforms = data["Platform"] || [];
        if (platforms.length > 0 && profilePlatform && !platforms.includes(profilePlatform)) {
            console.log("Skipping provisioning profile for platform " + platforms.join(", ")
                        + " (current platform " + profilePlatform + ")"
                        + ": " + profile.filePath);
            return false;
        }

        if (teamIdentifier
                && !data["TeamIdentifier"].includes(teamIdentifier)
                && data["TeamName"] !== teamIdentifier) {
            console.log("Skipping provisioning profile for team " + data["TeamIdentifier"]
                        + " (" + data["TeamName"] + ") (current team " + teamIdentifier + ")"
                        + ": " + profile.filePath);
            return false;
        }

        if (Date.parse(data["ExpirationDate"]) <= Date.now()) {
            console.log("Skipping expired provisioning profile: " + profile.filePath);
            return false;
        }

        // Filter development vs distribution profiles;
        // though the certificate common names check should have been sufficient
        var isDebug = buildVariant === "debug";
        if (data["Entitlements"]["get-task-allow"] !== isDebug) {
            console.log("Skipping provisioning profile for wrong debug mode: " + profile.filePath);
            return false;
        }

        var prefix = data["ApplicationIdentifierPrefix"];
        var fullAppId = data["Entitlements"]["application-identifier"];
        if ([prefix, bundleIdentifier].join(".") !== fullAppId
                && [prefix, "*"].join(".") !== fullAppId) {
            console.log("Skipping provisioning profile not matching full ("
                        + [prefix, bundleIdentifier].join(".") + ") or wildcard ("
                        + [prefix, "*"].join(".") + ") app ID (found " + fullAppId + "): "
                        + profile.filePath);
            return false;
        }

        return true;
    });

    // Sort by expiration date - sooner expiration dates come last
    profiles.sort(function(profileA, profileB) {
        var expA = Date.parse(profileA.data["ExpirationDate"]);
        var expB = Date.parse(profileB.data["ExpirationDate"]);
        if (expA < expB)
            return -1;
        if (expA > expB)
            return 1;
        return 0;
    });

    // Sort by application identifier - wildcard profiles come last
    profiles.sort(function(profileA, profileB) {
        var idA = profileA.data["Entitlements"]["application-identifier"];
        var idB = profileB.data["Entitlements"]["application-identifier"];
        if (!idA.endsWith(".*") && idB.endsWith(".*"))
            return -1;
        if (idA.endsWith(".*") && !idB.endsWith(".*"))
            return 1;
        return 0;
    });

    if (profiles.length) {
        console.log("Automatic provisioning using profile "
                    + profiles[0].data.UUID
                    + " ("
                    + profiles[0].data.TeamName
                    + " - "
                    + profiles[0].data.Name
                    + ") in product "
                    + product.name);
        return profiles[0];
    }
}

/**
  * Finds out the search paths for the `signtool.exe` utility supplied with
  * the Windows SDK's.
  */
function findBestSignToolSearchPaths(arch) {
    var searchPaths = [];
    var keys = [
                "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows",
                "HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\Microsoft SDKs\\Windows"
            ];
    for (var keyIndex = 0; keyIndex < keys.length; ++keyIndex) {
        var re = /^v([0-9]+)\.([0-9]+)$/;
        var groups = Utilities.nativeSettingGroups(keys[keyIndex]).filter(function(version) {
            return version.match(re);
        });

        groups.sort(function(a, b) {
            return Utilities.versionCompare(b.substring(1), a.substring(1));
        });

        function addSearchPath(searchPath) {
            if (File.exists(searchPath) && !searchPaths.includes(searchPath)) {
                searchPaths.push(searchPath);
                return true;
            }
            return false;
        }

        for (var groupIndex = 0; groupIndex < groups.length; ++groupIndex) {
            var fullKey = keys[keyIndex] + "\\" + groups[groupIndex];
            var fullVersion = Utilities.getNativeSetting(fullKey, "ProductVersion");
            if (fullVersion) {
                var installRoot = FileInfo.cleanPath(
                            Utilities.getNativeSetting(fullKey, "InstallationFolder"));
                if (installRoot) {
                    // Try to add the architecture-independent path at first.
                    var searchPath = FileInfo.joinPaths(installRoot, "App Certification Kit");
                    if (!addSearchPath(searchPath)) {
                        // Try to add the architecture-dependent paths at second.
                        var binSearchPath = FileInfo.joinPaths(installRoot, "bin/" + fullVersion);
                        if (!File.exists(binSearchPath)) {
                            binSearchPath += ".0";
                            if (!File.exists(binSearchPath))
                                continue;
                        }

                        function kitsArchitectureSubDirectory(arch) {
                            if (arch === "x86")
                                return "x86";
                            else if (arch === "x86_64")
                                return "x64";
                            else if (arch.startsWith("arm64"))
                                return "arm64";
                            else if (arch.startsWith("arm"))
                                return "arm";
                        }

                        var archDir = kitsArchitectureSubDirectory(arch);
                        searchPath = FileInfo.joinPaths(binSearchPath, archDir);
                        addSearchPath(searchPath);
                    }
                }
            }
        }
    }

    return searchPaths;
}

function prepareSign(project, product, inputs, outputs, input, output) {
    var cmd, cmds = [];

    if (!product.codesign.enableCodeSigning)
        return cmds;

    var isBundle = "bundle.content" in outputs;

    var artifacts = [];
    if (isBundle) {
        artifacts = [{
            filePath: FileInfo.joinPaths(product.destinationDirectory, product.bundle.bundleName),
            fileName: product.bundle.bundleName
        }];
    } else {
        artifacts = outputs["codesign.signed_artifact"];
    }
    var isProductBundle = product.bundle && product.bundle.isBundle;
    var shouldSignArtifact = !isProductBundle || isBundle;

    var enableCodeSigning = product.codesign.enableCodeSigning;
    if (enableCodeSigning) {
        var actualSigningIdentity = product.codesign._actualSigningIdentity;
        if (!actualSigningIdentity) {
            throw "No codesigning identities (i.e. certificate and private key pairs) matching “"
                    + product.codesign.signingIdentity + "” were found.";
        }

        // If this is a framework, we need to sign its versioned directory
        var subpath = "";
        if (isBundle) {
            var isFramework = product.bundle.packageType === "FMWK";
            if (isFramework) {
                subpath = product.bundle.contentsFolderPath;
                subpath = subpath.substring(product.bundle.bundleName.length);
            }
        }

        // The codesign tool behaves weirdly. It can sign a bundle with a single artifact, but if
        // say debug build variant is present, it starts complaining that it is not signed.
        // We could always sign everything, but again, in case of a framework (but not in case of
        // app or loadable bundle), codesign produces a warning that artifact is already signed.
        // So, we skip signing the release artifact and only sign if other build variants present.
        if (!shouldSignArtifact && artifacts.length == 1) {
            artifacts = [];
        }
        for (var i = 0; i < artifacts.length; ++i) {
            if (!shouldSignArtifact
                && artifacts[i].qbs && artifacts[i].qbs.buildVariant === "release") {
                continue;
            }
            var outputFilePath = artifacts[i].filePath;
            var outputFileName = artifacts[i].fileName;

            var args = ["--force", "--sign", actualSigningIdentity.SHA1];

            // If signingTimestamp is undefined or empty, do not specify the flag at all -
            // this uses the system-specific default behavior
            var signingTimestamp = product.codesign.signingTimestamp;
            if (signingTimestamp) {
                // If signingTimestamp is an empty string, specify the flag but do
                // not specify a value - this uses a default Apple-provided server
                var flag = "--timestamp";
                if (signingTimestamp)
                    flag += "=" + signingTimestamp;
                args.push(flag);
            }

            for (var j in inputs["codesign.xcent"]) {
                args.push("--entitlements", inputs["codesign.xcent"][j].filePath);
                break; // there should only be one
            }

            args = args.concat(product.codesign.codesignFlags || []);

            args.push(outputFilePath + subpath);
            cmd = new Command(product.codesign.codesignPath, args);
            cmd.description = "codesign " + outputFileName
                    + " (" + actualSigningIdentity.subjectInfo.CN + ")";
            cmd.outputFilePath = outputFilePath;
            cmd.stderrFilterFunction = function(stderr) {
                return stderr.replace(outputFilePath + ": replacing existing signature\n", "");
            };
            cmds.push(cmd);
        }
    }

    if (isBundle) {
        cmd = new Command("touch", ["-c", outputFilePath]);
        cmd.silent = true;
        cmds.push(cmd);
    }

    return cmds;
}

function signApkPackage(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var apkInput = inputs["android.package_unsigned"][0];
    var apkOutput = outputs["android.package"][0];
    var cmd;
    if (product.codesign.enableCodeSigning) {
        var args = ["sign",
                    "--ks", product.codesign.keystorePath,
                    "--ks-pass", "pass:" + product.codesign.keystorePassword,
                    "--ks-key-alias", product.codesign.keyAlias,
                    "--key-pass", "pass:" + product.codesign.keyPassword,
                    "--out", apkOutput.filePath,
                    apkInput.filePath];
        cmd = new Command(product.codesign.apksignerFilePath, args);
        cmd.description = "signing " + apkOutput.fileName;
    } else {
        cmd = new JavaScriptCommand();
        cmd.description = "copying without signing " + apkOutput.fileName;
        cmd.source = apkInput.filePath;
        cmd.target = apkOutput.filePath;
        cmd.silent = true;
        cmd.sourceCode = function() {
            // If enableCodeSigning is changed to false without any change to unsigned package then
            // the copy won't happen because of timestamps. So the target file needs file needs to
            // be removed to avoid it.
            File.remove(target);
            File.copy(source, target);
        }
    }
    return cmd;
}

function signAabPackage(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var aabInput = inputs["android.package_unsigned"][0];
    var aabOutput = outputs["android.package"][0];
    var cmd;
    if (product.codesign.enableCodeSigning) {
        args = ["-sigalg", "SHA1withRSA", "-digestalg", "SHA1",
                "-keystore", product.codesign.keystorePath,
                "-storepass", product.codesign.keystorePassword,
                "-keypass", product.codesign.keyPassword,
                "-signedjar", aabOutput.filePath,
                aabInput.filePath,
                product.codesign.keyAlias];
        cmd = new Command(product.codesign.jarsignerFilePath, args);
        cmd.description = "signing " + aabOutput.fileName;
    } else {
        cmd = new JavaScriptCommand();
        cmd.description = "copying without signing " + aabOutput.fileName;
        cmd.source = aabInput.filePath;
        cmd.target = aabOutput.filePath;
        cmd.silent = true;
        cmd.sourceCode = function() {
            // If enableCodeSigning is changed to false without any change to unsigned package then
            // the copy won't happen because of timestamps. So the target file needs file needs to
            // be removed to avoid it.
            File.remove(target);
            File.copy(source, target);
        }
    }
    return cmd;
}

function createDebugKeyStoreCommandString(keytoolFilePath, keystoreFilePath, keystorePassword,
                                          keyPassword, keyAlias) {
    var args = ["-genkey", "-keystore", keystoreFilePath, "-alias", keyAlias,
                "-storepass", keystorePassword, "-keypass", keyPassword, "-keyalg", "RSA",
                "-keysize", "2048", "-validity", "10000", "-dname",
                "CN=Android Debug,O=Android,C=US"];
    return Process.shellQuote(keytoolFilePath, args);
}

function prepareSigntool(project, product, inputs, outputs, input, output) {
    var cmd, cmds = [];

    if (!product.codesign.enableCodeSigning)
        return cmds;

    var args = ["sign"];

    var subjectName = product.codesign.subjectName;
    if (subjectName)
        args.push("/n", subjectName);

    var rootSubjectName = product.codesign.rootSubjectName;
    if (rootSubjectName)
        args.push("/r", rootSubjectName);

    var hashAlgorithm = product.codesign.hashAlgorithm;
    if (hashAlgorithm)
        args.push("/fd", hashAlgorithm);

    var signingTimestamp = product.codesign.signingTimestamp;
    if (signingTimestamp)
        args.push("/tr", signingTimestamp);

    var timestampAlgorithm = product.codesign.timestampAlgorithm;
    if (timestampAlgorithm)
        args.push("/td", timestampAlgorithm);

    var certificatePath = product.codesign.certificatePath;
    if (certificatePath)
        args.push("/f", certificatePath);

    var certificatePassword = product.codesign.certificatePassword;
    if (certificatePassword)
        args.push("/p", certificatePassword);

    var crossCertificatePath = product.codesign.crossCertificatePath;
    if (crossCertificatePath)
        args.push("/ac", crossCertificatePath);

    args = args.concat(product.codesign.codesignFlags || []);

    var outputArtifact = outputs["codesign.signed_artifact"][0];
    args.push(outputArtifact.filePath);

    cmd = new Command(product.codesign.codesignPath, args);
    cmd.description = "signing " + outputArtifact.fileName;
    cmd.highlight = "linker";
    cmds.push(cmd);
    return cmds;
}

function generateAppleProvisioningProfileOutputs()
{
    var artifacts = [];
    var provisioningProfiles = (inputs["codesign.provisioningprofile"] || [])
        .map(function (a) { return a.filePath; });
    var bestProfile = findBestProvisioningProfile(product, provisioningProfiles);
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

function generateAppleProvisioningProfileCommands(project, product, inputs, outputs, input, output,
                                                  explicitlyDependsOn)
{
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

function generateAppleEntitlementsCommands(project, product, inputs, outputs, input, output,
                                           explicitlyDependsOn)
{
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
