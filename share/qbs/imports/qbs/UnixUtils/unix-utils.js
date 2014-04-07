var FileInfo = loadExtension("qbs.FileInfo");

function soname(product, outputFilePath) {
    function majorVersion(version, defaultValue) {
        var n = parseInt(version, 10);
        return isNaN(n) ? defaultValue : n;
    }

    var outputFileName = FileInfo.fileName(outputFilePath);
    if (product.version) {
        var major = majorVersion(product.version);
        if (major) {
            return outputFileName.substr(0, outputFileName.length - product.version.length)
                    + major;
        }
    }
    return outputFileName;
}
