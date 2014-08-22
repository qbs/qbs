import qbs 1.0
import qbs.BundleTools
import qbs.DarwinTools
import qbs.File
import qbs.PathTools
import qbs.Process
import qbs.PropertyList
import qbs.TextFile
import qbs.ModUtils

UnixGCC {
    condition: false

    compilerDefines: ["__GNUC__", "__APPLE__"]
    dynamicLibrarySuffix: ".dylib"

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

    setupRunEnvironment: {
        var env;
        var installRoot = getEnv("QBS_INSTALL_ROOT");

        env = new ModUtils.EnvironmentVariable("DYLD_FRAMEWORK_PATH", qbs.pathListSeparator);
        env.append(FileInfo.joinPaths(installRoot, qbs.installPrefix, "Library", "Frameworks"));
        env.append(FileInfo.joinPaths(installRoot, qbs.installPrefix, "lib"));
        env.append(FileInfo.joinPaths(installRoot, qbs.installPrefix));
        env.set();

        env = new ModUtils.EnvironmentVariable("DYLD_LIBRARY_PATH", qbs.pathListSeparator);
        env.append(FileInfo.joinPaths(installRoot, qbs.installPrefix, "lib"));
        env.append(FileInfo.joinPaths(installRoot, qbs.installPrefix, "Library", "Frameworks"));
        env.append(FileInfo.joinPaths(installRoot, qbs.installPrefix));
        env.set();

        if (qbs.sysroot) {
            env = new ModUtils.EnvironmentVariable("DYLD_ROOT_PATH", qbs.pathListSeparator);
            env.append(qbs.sysroot);
            env.set();
        }
    }

    // private properties
    readonly property var buildEnv: {
        var env = {
            "EXECUTABLE_NAME": product.targetName,
            "LANG": "en_US.US-ASCII",
            "PRODUCT_NAME": product.name
        }
        if (minimumIosVersion)
            env["IPHONEOS_DEPLOYMENT_TARGET"] = minimumIosVersion;
        if (minimumOsxVersion)
            env["MACOSX_DEPLOYMENT_TARGET"] = minimumOsxVersion;
        return env;
    }

    readonly property var qmakeEnv: {
        return {
            "BUNDLEIDENTIFIER": "org.example." + DarwinTools.rfc1034(product.targetName),
            "EXECUTABLE": product.targetName,
            "ICON": product.targetName, // ### QBS-73
            "LIBRARY": product.targetName,
            "SHORT_VERSION": product.version || "1.0", // CFBundleShortVersionString
            "TYPEINFO": "????" // CFBundleSignature
        };
    }

    readonly property var defaultInfoPlist: {
        // Not a product type which uses Info.plists
        if (!product.type.contains("application") && !product.type.contains("applicationbundle") &&
            !product.type.contains("frameworkbundle") && !product.type.contains("bundle"))
            return undefined;

        var dict = {
            CFBundleDevelopmentRegion: "en", // default localization
            CFBundleDisplayName: product.targetName, // localizable
            CFBundleExecutable: product.targetName,
            CFBundleIdentifier: "org.example." + DarwinTools.rfc1034(product.targetName),
            CFBundleInfoDictionaryVersion: "6.0",
            CFBundleName: product.targetName, // short display name of the bundle, localizable
            CFBundlePackageType: BundleTools.packageType(product),
            CFBundleShortVersionString: product.version || "1.0", // "release" version number, localizable
            CFBundleSignature: "????", // legacy creator code in Mac OS Classic, can be ignored
            CFBundleVersion: product.version || "1.0.0" // build version number, must be 3 octets
        };

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

    readonly property path platformInfoPlist: platformPath ? [platformPath, "Info.plist"].join("/") : undefined
    readonly property path sdkSettingsPlist: sysroot ? [sysroot, "SDKSettings.plist"].join("/") : undefined
    readonly property path toolchainInfoPlist: toolchainInstallPath ? [toolchainInstallPath, "../../ToolchainInfo.plist"].join("/") : undefined

    Rule {
        multiplex: true
        inputs: ["infoplist"]

        Artifact {
            filePath: product.destinationDirectory + "/" + BundleTools.pkgInfoPath(product)
            fileTags: ["pkginfo"]
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
        multiplex: true
        inputs: ["qbs", "partial_infoplist"]

        Artifact {
            filePath: product.destinationDirectory + "/" + BundleTools.infoPlistPath(product)
            fileTags: ["infoplist"]
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
            cmd.platformPath = product.moduleProperty("cpp", "platformPath");
            cmd.toolchainInstallPath = product.moduleProperty("cpp", "toolchainInstallPath");
            cmd.sysroot = product.moduleProperty("qbs", "sysroot");
            cmd.buildEnv = product.moduleProperty("cpp", "buildEnv");
            cmd.qmakeEnv = product.moduleProperty("cpp", "qmakeEnv");

            cmd.platformInfoPlist = product.moduleProperty("cpp", "platformInfoPlist");
            cmd.sdkSettingsPlist = product.moduleProperty("cpp", "sdkSettingsPlist");
            cmd.toolchainInfoPlist = product.moduleProperty("cpp", "toolchainInfoPlist");

            cmd.osBuildVersion = product.moduleProperty("qbs", "hostOSBuildVersion");

            cmd.sourceCode = function() {
                var plist, process, key;

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

                    // Add keys from platform's Info.plist if not already present
                    var platformInfo = {};
                    if (platformPath) {
                        if (File.exists(platformInfoPlist)) {
                            plist = new PropertyList();
                            try {
                                plist.readFromFile(platformInfoPlist);
                                platformInfo = JSON.parse(plist.toJSONString());
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
                                sdkSettings = JSON.parse(plist.toJSONString());
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
                            toolchainInfo = JSON.parse(plist.toJSONString());
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

                if (infoPlistFormat === "same-as-input" && infoPlistFile)
                    infoPlistFormat = BundleTools.infoPlistFormat(infoPlistFile);

                var validFormats = [ "xml1", "binary1", "json" ];
                if (!validFormats.contains(infoPlistFormat))
                    throw("Invalid Info.plist format " + infoPlistFormat + ". " +
                          "Must be in [xml1, binary1, json].");

                // Write the plist contents in the format appropriate for the current platform
                plist = new PropertyList();
                try {
                    plist.readFromString(JSON.stringify(aggregatePlist));
                    plist.writeToFile(outputs.infoplist[0].filePath, infoPlistFormat);
                } finally {
                    plist.clear();
                }
            }
            return cmd;
        }
    }

    Rule {
        condition: product.moduleProperty("cpp", "buildDsym")
        inputs: ["application"]

        Artifact {
            filePath: product.destinationDirectory + "/" + PathTools.dwarfDsymFileName(product)
            fileTags: ["application_dsym"]
        }

        prepare: {
            var cmd = new Command("dsymutil", [
                                      "--out=" + outputs.application_dsym[0].filePath,
                                      input.filePath
                                  ]);
            cmd.description = "generating dsym for " + product.name;
            cmd.highlight = "codegen";
            return cmd;
        }
    }

    Rule {
        multiplex: true
        inputs: ["application", "infoplist", "pkginfo", "icns", "application_dsym",
                 "compiled_nib", "compiled_storyboard", "compiled_assetcatalog",
                 "resourcerules", "ipa"]

        Artifact {
            filePath: product.destinationDirectory + "/" + BundleTools.wrapperName(product)
            fileTags: ["applicationbundle"]
        }

        prepare: {
            // This command is intentionally empty; it just lets the user know a bundle has been made
            var cmd = new JavaScriptCommand();
            cmd.description = "creating app bundle";
            cmd.highlight = "codegen";
            return cmd;
        }
    }

    Rule {
        multiplex: true
        inputs: ["dynamiclibrary", "infoplist", "pkginfo", "icns", "dynamiclibrary_dsym",
                 "compiled_nib", "compiled_storyboard", "compiled_assetcatalog"]

        Artifact {
            filePath: product.destinationDirectory + "/" + BundleTools.wrapperName(product)
            fileTags: ["frameworkbundle"]
        }

        prepare: {
            var commands = [];
            var cmd = new Command("ln", ["-sf", BundleTools.frameworkVersion(product), "Current"]);
            cmd.workingDirectory = output.filePath + "/Versions";
            cmd.description = "creating framework " + product.targetName;
            cmd.highlight = "codegen";
            commands.push(cmd);

            cmd = new Command("ln", ["-sf", "Versions/Current/Headers", "Headers"]);
            cmd.workingDirectory = output.filePath;
            cmd.silent = true;
            commands.push(cmd);

            cmd = new Command("ln", ["-sf", "Versions/Current/Resources", "Resources"]);
            cmd.workingDirectory = output.filePath;
            cmd.silent = true;
            commands.push(cmd);

            cmd = new Command("ln", ["-sf", "Versions/Current/" + product.targetName, product.targetName]);
            cmd.workingDirectory = output.filePath;
            cmd.silent = true;
            commands.push(cmd);
            return commands;
        }
    }
}
