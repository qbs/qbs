var BundleTools = loadExtension("qbs.BundleTools");

function applicationFileName(product) {
    return product.moduleProperty("cpp", "executablePrefix")
         + product.targetName
         + product.moduleProperty("cpp", "executableSuffix");
}

function applicationFilePath(product) {
    if (BundleTools.isBundleProduct(product))
        return BundleTools.executablePath(product);
    else
        return applicationFileName(product);
}

function staticLibraryFileName(product) {
    return product.moduleProperty("cpp", "staticLibraryPrefix")
         + product.targetName
         + product.moduleProperty("cpp", "staticLibrarySuffix");
}

function staticLibraryFilePath(product) {
    if (BundleTools.isBundleProduct(product))
        return BundleTools.executablePath(product);
    else
        return staticLibraryFileName(product);
}

function dynamicLibraryFileName(product, version, maxParts) {
    // If no override version was given, use the product's version
    // We specifically want to differentiate between undefined and i.e.
    // empty string as empty string should be taken to mean "no version"
    // and undefined should be taken to mean "use the product's version"
    // (which could still end up being "no version")
    if (version === undefined)
        version = product.moduleProperty("cpp", "internalVersion");

    // If we have a version number, potentially strip off some components
    maxParts = parseInt(maxParts, 10);
    if (maxParts === 0)
        version = undefined;
    else if (maxParts && version)
        version = version.split('.').slice(0, maxParts).join('.');

    // Start with prefix + name (i.e. libqbs, qbs)
    var fileName = product.moduleProperty("cpp", "dynamicLibraryPrefix") + product.targetName;

    // For Darwin platforms, append the version number if there is one (i.e. libqbs.1.0.0)
    var targetOS = product.moduleProperty("qbs", "targetOS");
    if (version && targetOS.contains("darwin")) {
        fileName += "." + version;
        version = undefined;
    }

    // Append the suffix (i.e. libqbs.1.0.0.dylib, libqbs.so, qbs.dll)
    fileName += product.moduleProperty("cpp", "dynamicLibrarySuffix");

    // For non-Darwin Unix platforms, append the version number if there is one (i.e. libqbs.so.1.0.0)
    if (version && targetOS.contains("unix") && !targetOS.contains("darwin"))
        fileName += "." + version;

    return fileName;
}

function dynamicLibraryFilePath(product, version, maxParts) {
    if (BundleTools.isBundleProduct(product))
        return BundleTools.executablePath(product, version);
    else
        return dynamicLibraryFileName(product, version, maxParts);
}

function importLibraryFilePath(product) {
    return product.moduleProperty("cpp", "dynamicLibraryPrefix")
         + product.targetName
         + product.moduleProperty("cpp", "dynamicLibraryImportSuffix");
}

// DWARF_DSYM_FILE_NAME
// Filename of the target's corresponding dSYM file
function dwarfDsymFileName(product) {
    if (BundleTools.isBundleProduct(product))
        return BundleTools.wrapperName(product) + ".dSYM";
    else if (product.type.contains("application"))
        return applicationFileName(product) + ".dSYM";
    else if (product.type.contains("dynamiclibrary"))
        return dynamicLibraryFileName(product) + ".dSYM";
    else if (product.type.contains("staticlibrary"))
        return staticLibraryFileName(product) + ".dSYM";
    else
        return product.targetName + ".dSYM";
}

// Returns whether the string looks like a library filename
function isLibraryFileName(product, fileName, prefix, suffixes, isShared) {
    var suffix, i;
    var os = product.moduleProperty("qbs", "targetOS");
    for (i = 0; i < suffixes.length; ++i) {
        suffix = suffixes[i];
        if (isShared && os.contains("unix") && !os.contains("darwin"))
            suffix += "(\\.[0-9]+){0,3}";
        if (fileName.match("^" + prefix + ".+?\\" + suffix + "$"))
            return true;
    }
    return false;
}
