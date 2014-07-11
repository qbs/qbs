var ModUtils = loadExtension("qbs.ModUtils");
var Process = loadExtension("qbs.Process");
var PropertyList = loadExtension("qbs.PropertyList");

function prepareIbtoold(product, input, outputs) {
    var args = [];

    var outputFormat = ModUtils.moduleProperty(input, "outputFormat");
    if (!["binary1", "xml1", "human-readable-text"].contains(outputFormat))
        throw("Invalid ibtoold output format: " + outputFormat + ". " +
              "Must be in [binary1, xml1, human-readable-text].");

    args.push("--output-format", outputFormat);

    if (ModUtils.moduleProperty(input, "warnings"))
        args.push("--warnings");

    if (ModUtils.moduleProperty(input, "errors"))
        args.push("--errors");

    if (ModUtils.moduleProperty(input, "notices"))
        args.push("--notices");

    if (input.fileTags.contains("assetcatalog")) {
    } else {
        var sysroot = product.moduleProperty("qbs", "sysroot");
        if (sysroot)
            args.push("--sdk", sysroot);

        args.push("--flatten", ModUtils.moduleProperty(input, "flatten") ? 'YES' : 'NO');
    }

    // --minimum-deployment-target was introduced in Xcode 5.0
    if (ModUtils.moduleProperty(input, "ibtoolVersionMajor") >= 5) {
        if (product.moduleProperty("cpp", "minimumOsxVersion")) {
            args.push("--minimum-deployment-target");
            args.push(product.moduleProperty("cpp", "minimumOsxVersion"));
        }

        if (product.moduleProperty("cpp", "minimumIosVersion")) {
            args.push("--minimum-deployment-target");
            args.push(product.moduleProperty("cpp", "minimumIosVersion"));
        }
    }

    return args;
}

function ibtoolVersion(ibtool) {
    var process;
    var version;
    try {
        process = new Process();
        if (process.exec(ibtool, ["--version"], true) !== 0)
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
    return version;
}
