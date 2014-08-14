var BundleTools = loadExtension("qbs.BundleTools");
var FileInfo = loadExtension("qbs.FileInfo");

function soname(product, outputFileName) {
    if (product.moduleProperty("qbs", "targetOS").contains("darwin")) {
        if (BundleTools.isBundleProduct(product))
            outputFileName = BundleTools.executablePath(product);
        var prefix = product.moduleProperty("cpp", "installNamePrefix");
        if (prefix)
            outputFileName = FileInfo.joinPaths(prefix, outputFileName);
        return outputFileName;
    }

    function majorVersion(version, defaultValue) {
        var n = parseInt(version, 10);
        return isNaN(n) ? defaultValue : n;
    }

    if (product.version) {
        var major = majorVersion(product.version);
        if (major) {
            return outputFileName.substr(0, outputFileName.length - product.version.length)
                    + major;
        }
    }
    return outputFileName;
}
