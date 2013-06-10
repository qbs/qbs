import qbs 1.0
import qbs.fileinfo as FileInfo
import '../utils.js' as ModUtils
import "bundle-tools.js" as BundleTools
import "darwin-tools.js" as DarwinTools

UnixGCC {
    condition: false

    compilerDefines: ["__GNUC__", "__APPLE__"]
    dynamicLibrarySuffix: ".dylib"

    property path infoPlistFile
    property var infoPlist
    property bool processInfoPlist: true
    property bool buildDsym: qbs.buildVariant === "release"

    readonly property var defaultInfoPlist: {
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

        if (product.type.indexOf("applicationbundle") !== -1)
            dict["CFBundleIconFile"] = product.targetName;

        if (qbs.targetOS === "osx" && minimumOsxVersion)
            dict["LSMinimumSystemVersion"] = minimumOsxVersion;

        if (qbs.targetOS === "ios") {
            dict["LSRequiresIPhoneOS"] = true;

            // architectures supported, to support iPhone 3G for example one has to add
            // armv6 to the list and also compile for it (with Xcode 4.4.1 or earlier)
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

    Rule {
        multiplex: true
        inputs: ["infoplist"]
        explicitlyDependsOn: ["infoplist"]

        Artifact {
            fileName: product.destinationDirectory + "/" + BundleTools.pkgInfoPath(product)
            fileTags: ["pkginfo"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating PkgInfo";
            cmd.highlight = "codegen";
            cmd.sourceCode = function() {
                var infoPlist = BundleTools.infoPlistContents(inputs.infoplist[0].fileName);

                var pkgType = infoPlist['CFBundlePackageType'];
                if (!pkgType)
                    throw("CFBundlePackageType not found in Info.plist; this should not happen");

                var pkgSign = infoPlist['CFBundleSignature'];
                if (!pkgSign)
                    throw("CFBundleSignature not found in Info.plist; this should not happen");

                var pkginfo = new TextFile(outputs.pkginfo[0].fileName, TextFile.WriteOnly);
                pkginfo.write(pkgType + pkgSign);
                pkginfo.close();
            }
            return cmd;
        }
    }

    Rule {
        multiplex: true
        inputs: ["qbs"]

        Artifact {
            fileName: product.destinationDirectory + "/" + BundleTools.infoPlistPath(product)
            fileTags: ["infoplist"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating Info.plist";
            cmd.highlight = "codegen";
            cmd.infoPlistFile = ModUtils.moduleProperty(product, "infoPlistFile");
            cmd.infoPlist = ModUtils.moduleProperty(product, "infoPlist") || {};
            cmd.processInfoPlist = ModUtils.moduleProperty(product, "processInfoPlist");
            cmd.platformPath = product.moduleProperty("cpp", "platformPath");
            cmd.sourceCode = function() {
                var process, key;

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
                    if (platformPath) {
                        process = new Process();
                        process.start("plutil", ["-convert", "json", "-o", "-",
                                                 [platformPath, "Info.plist"].join('/')]);
                        process.waitForFinished();
                        platformInfo = JSON.parse(process.readAll());

                        var additionalProps = platformInfo["AdditionalInfo"];
                        for (key in additionalProps) {
                            if (additionalProps.hasOwnProperty(key) && !(key in aggregatePlist)) // override infoPlist?
                                aggregatePlist[key] = defaultValues[key];
                        }

                        if (product.moduleProperty("qbs", "targetOS") === "ios") {
                            key = "UIDeviceFamily";
                            if (key in platformInfo && !(key in aggregatePlist))
                                aggregatePlist[key] = platformInfo[key];
                        }
                    } else {
                        print("Missing platformPath property");
                    }

                    process = new Process();
                    process.start("sw_vers", ["-buildVersion"]);
                    process.waitForFinished();
                    aggregatePlist["BuildMachineOSBuild"] = process.readAll().trim();
                }

                // Write the plist contents as JSON
                var infoplist = new TextFile(outputs.infoplist[0].fileName, TextFile.WriteOnly);
                infoplist.write(JSON.stringify(aggregatePlist));
                infoplist.close();

                // Convert the written file to the format appropriate for the current platform
                process = new Process();
                process.start("plutil", ["-convert",
                                        product.moduleProperty("qbs", "targetOS") === "ios"
                                            ? "binary1" : "xml1",
                                        outputs.infoplist[0].fileName]);
                process.waitForFinished();
            }
            return cmd;
        }
    }

    Rule {
        condition: product.moduleProperty("cpp", "buildDsym")
        inputs: ["application"]

        Artifact {
            fileName: product.destinationDirectory + "/" + PathTools.dwarfDsymFileName()
            fileTags: ["dsym"]
        }

        prepare: {
            var cmd = new Command("dsymutil", ["--out=" + outputs.dsym[0].fileName, input.fileName]);
            cmd.description = "generating dsym";
            cmd.highlight = "codegen";
            return cmd;
        }
    }

    Rule {
        multiplex: true
        inputs: ["application", "infoplist", "pkginfo", "dsym", "nib", "resourcerules", "ipa"]

        Artifact {
            fileName: product.destinationDirectory + "/" + BundleTools.wrapperName(product)
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
        inputs: ["dynamiclibrary", "infoplist", "pkginfo", "dsym", "nib"]

        Artifact {
            fileName: product.destinationDirectory + "/" + BundleTools.wrapperName(product)
            fileTags: ["frameworkbundle"]
        }

        prepare: {
            var commands = [];
            var cmd = new Command("ln", ["-s", BundleTools.frameworkVersion(product), "Current"]);
            cmd.workingDirectory = output.fileName + "/Versions";
            cmd.description = "creating framework " + product.targetName;
            cmd.highlight = "codegen";
            commands.push(cmd);

            cmd = new Command("ln", ["-s", "Versions/Current/Headers", "Headers"]);
            cmd.workingDirectory = output.fileName;
            commands.push(cmd);

            cmd = new Command("ln", ["-s", "Versions/Current/Resources", "Resources"]);
            cmd.workingDirectory = output.fileName;
            commands.push(cmd);

            cmd = new Command("ln", ["-s", "Versions/Current/" + product.targetName, product.targetName]);
            cmd.workingDirectory = output.fileName;
            commands.push(cmd);
            return commands;
        }
    }

    FileTagger {
        pattern: "*.xib"
        fileTags: ["xib"]
    }

    Rule {
        inputs: ["xib"]
        explicitlyDependsOn: ["infoplist"]

        Artifact {
            fileName: {
                var path = product.destinationDirectory;

                var xibFilePath = input.baseDir + '/' + input.fileName;
                var key = DarwinTools.localizationKey(xibFilePath);
                if (key) {
                    path += '/' + BundleTools.localizedResourcesFolderPath(product, key);
                    var subPath = DarwinTools.relativeResourcePath(xibFilePath);
                    if (subPath && subPath !== '.')
                        path += '/' + subPath;
                } else {
                    path += '/' + BundleTools.unlocalizedResourcesFolderPath(product);
                    path += '/' + input.baseDir;
                }

                return path + '/' + input.completeBaseName + ".nib";
            }

            fileTags: ["nib"]
        }

        prepare: {
            var cmd = new Command("ibtool", [
                                      '--output-format', 'human-readable-text',
                                      '--warnings', '--errors', '--notices',
                                      '--compile', output.fileName, input.fileName,
                                      '--sdk', product.moduleProperty("qbs", "sysroot")]);

            cmd.description = 'ibtool ' + FileInfo.fileName(input.fileName);

            // Also display the language name of the XIB being compiled if it has one
            var localizationKey = DarwinTools.localizationKey(input.fileName);
            if (localizationKey)
                cmd.description += ' (' + localizationKey + ')';

            cmd.highlight = 'compiler';
            return cmd;
        }
    }
}
