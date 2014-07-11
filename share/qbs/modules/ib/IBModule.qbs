import qbs
import qbs.BundleTools
import qbs.DarwinTools
import qbs.FileInfo
import qbs.ModUtils
import 'ib.js' as Ib

Module {
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

    // private properties
    property string outputFormat: "human-readable-text"
    property string appleIconSuffix: ".icns"
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
}
