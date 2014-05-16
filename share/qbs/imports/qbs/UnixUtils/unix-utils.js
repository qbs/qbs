function soname(product, outputFileName) {
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
