var FileInfo = loadExtension("qbs.FileInfo");

function soname(product, outputFileName) {
    if (product.moduleProperty("qbs", "targetOS").contains("darwin")) {
        if (product.moduleProperty("bundle", "isBundle"))
            outputFileName = product.moduleProperty("bundle", "executablePath");
        var prefix = product.moduleProperty("cpp", "installNamePrefix");
        if (prefix)
            outputFileName = FileInfo.joinPaths(prefix, outputFileName);
        return outputFileName;
    }

    function majorVersion(version, defaultValue) {
        var n = parseInt(version, 10);
        return isNaN(n) ? defaultValue : n;
    }

    var version = product.moduleProperty("cpp", "internalVersion");
    if (version) {
        var major = majorVersion(version);
        if (major) {
            return outputFileName.substr(0, outputFileName.length - version.length)
                    + major;
        }
    }
    return outputFileName;
}
