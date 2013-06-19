function applicationFileName()
{
    return ModUtils.moduleProperty(product, "executablePrefix")
         + product.targetName
         + ModUtils.moduleProperty(product, "executableSuffix");
}

function applicationFilePath()
{
    if (BundleTools.isBundleProduct(product))
        return BundleTools.executablePath(product);
    else
        return applicationFileName();
}

function staticLibraryFileName()
{
    return ModUtils.moduleProperty(product, "staticLibraryPrefix")
         + product.targetName
         + ModUtils.moduleProperty(product, "staticLibrarySuffix");
}

function staticLibraryFilePath()
{
    if (BundleTools.isBundleProduct(product))
        return BundleTools.executablePath(product);
    else
        return staticLibraryFileName();
}

function dynamicLibraryFileName(version, maxParts)
{
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
    var fileName = ModUtils.moduleProperty(product, "dynamicLibraryPrefix") + product.targetName;

    // For Darwin platforms, append the version number if there is one (i.e. libqbs.1.0.0)
    var targetOS = product.moduleProperty("qbs", "targetOS");
    if (version && targetOS.contains("darwin")) {
        fileName += "." + version;
        version = undefined;
    }

    // Append the suffix (i.e. libqbs.1.0.0.dylib, libqbs.so, qbs.dll)
    fileName += ModUtils.moduleProperty(product, "dynamicLibrarySuffix");

    // For non-Darwin Unix platforms, append the version number if there is one (i.e. libqbs.so.1.0.0)
    if (version && targetOS.contains("unix") && !targetOS.contains("darwin"))
        fileName += "." + version;

    return fileName;
}

function dynamicLibraryFilePath(version, maxParts)
{
    if (BundleTools.isBundleProduct(product))
        return BundleTools.executablePath(product, version);
    else
        return dynamicLibraryFileName(version, maxParts);
}

function importLibraryFilePath()
{
    return ModUtils.moduleProperty(product, "dynamicLibraryPrefix")
         + product.targetName
         + ModUtils.moduleProperty(product, "dynamicLibraryImportSuffix");
}

// DWARF_DSYM_FILE_NAME
// Filename of the target's corresponding dSYM file
function dwarfDsymFileName()
{
    if (BundleTools.isBundleProduct(product))
        return BundleTools.wrapperName(product) + ".dSYM";
    else if (product.type.contains("application"))
        return applicationFileName() + ".dSYM";
    else if (product.type.contains("dynamiclibrary"))
        return dynamicLibraryFileName() + ".dSYM";
    else if (product.type.contains("staticlibrary"))
        return staticLibraryFileName() + ".dSYM";
    else
        return product.targetName + ".dSYM";
}
