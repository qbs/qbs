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

function dynamicLibraryFileName(version)
{
    // If no override version was given, use the product's version
    if (!version)
        version = product.version;

    // Start with prefix + name (i.e. libqbs, qbs)
    var fileName = ModUtils.moduleProperty(product, "dynamicLibraryPrefix") + product.targetName;

    // For Darwin platforms, append the version number if there is one (i.e. libqbs.1.0.0)
    if (version && product.moduleProperty("qbs", "targetPlatform").indexOf("darwin") !== -1) {
        fileName += "." + version;
        version = undefined;
    }

    // Append the suffix (i.e. libqbs.1.0.0.dylib, libqbs.so, qbs.dll)
    fileName += ModUtils.moduleProperty(product, "dynamicLibrarySuffix");

    // For non-Darwin Unix platforms, append the version number if there is one (i.e. libqbs.so.1.0.0)
    if (version && product.moduleProperty("qbs", "targetPlatform").indexOf("unix") !== -1)
        fileName += "." + version;

    return fileName;
}

function dynamicLibraryFilePath(version)
{
    if (BundleTools.isBundleProduct(product))
        return BundleTools.executablePath(product, version);
    else
        return dynamicLibraryFileName(version);
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
    else if (product.type.indexOf("application") !== -1)
        return applicationFileName() + ".dSYM";
    else if (product.type.indexOf("dynamiclibrary") !== -1)
        return dynamicLibraryFileName() + ".dSYM";
    else if (product.type.indexOf("staticlibrary") !== -1)
        return staticLibraryFileName() + ".dSYM";
    else
        return product.targetName + ".dSYM";
}
