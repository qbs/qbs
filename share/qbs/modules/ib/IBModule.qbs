import qbs 1.0
import qbs.FileInfo
import qbs.ModUtils
import qbs.Process
import qbs.PropertyList
import "../cpp/bundle-tools.js" as BundleTools
import "../cpp/darwin-tools.js" as DarwinTools

Module {
    condition: qbs.hostOS.contains("darwin") && qbs.targetOS.contains("darwin")

    property string outputFormat: "human-readable-text"
    property bool flatten: true // false to preserve editability of the resulting nib file
    property bool autoUpgrade: false

    property bool warnings: true
    property bool errors: true
    property bool notices: true

    property stringList flags

    FileTagger {
        patterns: ["*.nib", "*.xib"]
        fileTags: ["nib"]
    }

    Rule {
        inputs: ["nib"]
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

            fileTags: ["compiled_nib"]
        }

        prepare: {
            var args = [];

            var flags = ModUtils.moduleProperty(product, "flags");
            if (flags)
                args = args.concat(flags);

            var outputFormat = ModUtils.moduleProperty(product, "outputFormat");
            if (!["binary1", "xml1", "human-readable-text"].contains(outputFormat))
                throw("Invalid ibtool output format: " + outputFormat + ". " +
                      "Must be in [binary1, xml1, human-readable-text].");

            args.push("--output-format");
            args.push(outputFormat);

            args.push("--flatten");
            args.push(ModUtils.moduleProperty(product, "flatten") ? 'YES' : 'NO');

            if (ModUtils.moduleProperty(product, "autoUpgrade"))
                args.push("--upgrade");

            if (ModUtils.moduleProperty(product, "warnings"))
                args.push("--warnings");

            if (ModUtils.moduleProperty(product, "errors"))
                args.push("--errors");

            if (ModUtils.moduleProperty(product, "notices"))
                args.push("--notices");

            var process;
            var version;
            try {
                process = new Process();
                if (process.exec("ibtool", ["--version"], true) !== 0)
                    print(process.readStdErr());

                var propertyList = new PropertyList();
                try {
                    propertyList.readFromString(process.readStdOut());

                    var plist = JSON.parse(propertyList.toJSONString());
                    if (plist)
                        plist = plist["com.apple.ibtool.version"];
                    if (plist)
                        version = plist["short-bundle-version"];
                } finally {
                    propertyList.clear();
                }
            } finally {
                process.close();
            }

            // --minimum-deployment-target was introduced in Xcode 5.0
            if (version && parseInt(version.split('.')[0], 10) >= 5) {
                if (product.moduleProperty("cpp", "minimumOsxVersion")) {
                    args.push("--minimum-deployment-target");
                    args.push(product.moduleProperty("cpp", "minimumOsxVersion"));
                }

                if (product.moduleProperty("cpp", "minimumIosVersion")) {
                    args.push("--minimum-deployment-target");
                    args.push(product.moduleProperty("cpp", "minimumIosVersion"));
                }
            }

            if (product.moduleProperty("qbs", "sysroot")) {
                args.push("--sdk");
                args.push(product.moduleProperty("qbs", "sysroot"));
            }

            args.push("--compile");
            args.push(output.filePath);
            args.push(input.filePath);

            var cmd = new Command("ibtool", args);
            cmd.description = 'ibtool ' + FileInfo.fileName(input.filePath);

            // Also display the language name of the XIB being compiled if it has one
            var localizationKey = DarwinTools.localizationKey(input.filePath);
            if (localizationKey)
                cmd.description += ' (' + localizationKey + ')';

            cmd.highlight = 'compiler';
            return cmd;
        }
    }
}
