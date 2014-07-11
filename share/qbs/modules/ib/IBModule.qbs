import qbs
import qbs.BundleTools
import qbs.DarwinTools
import qbs.FileInfo
import qbs.ModUtils
import 'ib.js' as Ib

Module {
    Depends { name: "cpp" } // to put toolchainInstallPath in the PATH for actool

    condition: qbs.hostOS.contains("darwin") && qbs.targetOS.contains("darwin")

    property bool warnings: true
    property bool errors: true
    property bool notices: true

    property stringList flags

    // iconutil specific
    property string iconutilName: "iconutil"
    property string iconutilPath: iconutilName

    // XIB/NIB specific
    property string ibtoolName: "ibtool"
    property string ibtoolPath: ibtoolName
    property bool flatten: true

    // Asset catalog specific
    property string actoolName: "actool"
    property string actoolPath: actoolName
    property string appIconName
    property string launchImageName
    property bool compressPngs: true

    // private properties
    property string outputFormat: "human-readable-text"
    property string appleIconSuffix: ".icns"
    property string compiledAssetCatalogSuffix: ".car"
    property string compiledNibSuffix: ".nib"

    property string ibtoolVersion: { return Ib.ibtoolVersion(ibtoolPath); }
    property var ibtoolVersionParts: ibtoolVersion ? ibtoolVersion.split('.').map(function(item) { return parseInt(item, 10); }) : []
    property int ibtoolVersionMajor: ibtoolVersionParts[0]
    property int ibtoolVersionMinor: ibtoolVersionParts[1]
    property int ibtoolVersionPatch: ibtoolVersionParts[2]

    validate: {
        var validator = new ModUtils.PropertyValidator("ib");
        validator.setRequiredProperty("ibtoolVersion", ibtoolVersion);
        validator.setRequiredProperty("ibtoolVersionMajor", ibtoolVersionMajor);
        validator.setRequiredProperty("ibtoolVersionMinor", ibtoolVersionMinor);
        validator.setRequiredProperty("ibtoolVersionPatch", ibtoolVersionPatch);
        validator.addVersionValidator("ibtoolVersion", ibtoolVersion, 3, 3);
        validator.addRangeValidator("ibtoolVersionMajor", ibtoolVersionMajor, 1);
        validator.addRangeValidator("ibtoolVersionMinor", ibtoolVersionMinor, 0);
        validator.addRangeValidator("ibtoolVersionPatch", ibtoolVersionPatch, 0);
        validator.validate();
    }

    FileTagger {
        patterns: ["*.iconset"] // bundle
        fileTags: ["iconset"]
    }

    FileTagger {
        patterns: ["*.nib", "*.xib"]
        fileTags: ["nib"]
    }

    FileTagger {
        patterns: ["*.xcassets"] // bundle
        fileTags: ["assetcatalog"]
    }

    Rule {
        inputs: ["iconset"]

        Artifact {
            filePath: {
                var outputDirectory = BundleTools.isBundleProduct(product)
                        ? BundleTools.unlocalizedResourcesFolderPath(product)
                        : product.destinationDirectory;
                return FileInfo.joinPaths(outputDirectory, input.completeBaseName + ModUtils.moduleProperty(product, "appleIconSuffix"))
            }
            fileTags: ["icns"]
        }

        prepare: {
            var args = ["--convert", "icns", "--output", output.filePath, input.filePath];
            var cmd = new Command(ModUtils.moduleProperty(product, "iconutilPath"), args);
            cmd.description = ModUtils.moduleProperty(product, "iconutilName") + ' ' + input.fileName;
            return cmd;
        }
    }

    Rule {
        inputs: ["nib"]
        explicitlyDependsOn: ["infoplist"]

        Artifact {
            filePath: {
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

                return path + '/' + input.completeBaseName + ModUtils.moduleProperty(product, "compiledNibSuffix");
            }

            fileTags: ["compiled_nib"]
        }

        prepare: {
            var args = Ib.prepareIbtoold(product, input, outputs);

            var flags = ModUtils.moduleProperty(input, "flags");
            if (flags)
                args = args.concat(flags);

            args.push("--compile", output.filePath);
            args.push(input.filePath);

            var cmd = new Command(ModUtils.moduleProperty(input, "ibtoolPath"), args);
            cmd.description = ModUtils.moduleProperty(input, "ibtoolName") + ' ' + input.fileName;

            // Also display the language name of the nib being compiled if it has one
            var localizationKey = DarwinTools.localizationKey(input.filePath);
            if (localizationKey)
                cmd.description += ' (' + localizationKey + ')';

            cmd.highlight = 'compiler';
            return cmd;
        }
    }

    Rule {
        inputs: ["assetcatalog"]

        // We only return one artifact, as this is a little complicated...
        // actool takes an output *directory*, and in this directory it will
        // potentially output "Assets.car" and/or one or more additional files.
        // We can discover which files were written in an easily parseable manner
        // through use of --output-format xml1, but we have a chicken and egg problem
        // in that we only gain that information *after* running the compilation, so
        // if we want to know in advance which artifacts are generated we have to run
        // the compilation twice which probably isn't worth it.
        outputArtifacts: {
            var outputDirectory = BundleTools.isBundleProduct(product)
                    ? BundleTools.unlocalizedResourcesFolderPath(product)
                    : product.destinationDirectory;
            return [{
                filePath: FileInfo.joinPaths(outputDirectory, "Assets" + ModUtils.moduleProperty(product, "compiledAssetCatalogSuffix")),
                fileTags: ["compiled_assetcatalog"]
            },
            {
                filePath: FileInfo.joinPaths(product.destinationDirectory, "assetcatalog_generated_info.plist"),
                fileTags: ["partial_infoplist"]
            }];
        }

        outputFileTags: ["compiled_assetcatalog", "partial_infoplist"]

        // Just a note, the man page for actool is somewhat outdated (probably forgotten to be updated late in the development cycle).
        // It mentions the file extension .assetcatalog (which isn't used by Xcode), the --write option does not exist, and the example
        // invocation near the bottom of the man page doesn't work at all.
        // There's also the undocumented --export-dependency-info <output.txt> which is used by Xcode and generated a \0x00\0x02-delimited
        // file (yes, really) that contains the output file names, identical to the output of actool itself (what's the point?).
        prepare: {
            var args = Ib.prepareIbtoold(product, input, outputs);

            var flags = ModUtils.moduleProperty(input, "flags");
            if (flags)
                args = args.concat(flags);

            var outputPath = FileInfo.path(outputs.compiled_assetcatalog[0].filePath);

            args.push("--compile");
            args.push(outputPath);
            args.push(input.filePath);

            var cmd = new Command(ModUtils.moduleProperty(input, "actoolPath"), args);
            cmd.description = ModUtils.moduleProperty(input, "actoolName") + ' ' + input.fileName;
            cmd.highlight = "compiler";
            cmd.stdoutFilterFunction = function(stdout) {
                stdout = stdout.replace("/* com.apple.actool.compilation-results */\n", "");
                return stdout.split("\n").filter(function(line) {
                    return line.length > 0 /*&& line.indexOf(outputPath) !== 0*/;
                }).join("\n");
            }

            return cmd;
        }
    }
}
