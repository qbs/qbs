// NOTE: QBS and Xcode's "target" and "product" names are reversed

var PropertyList = loadExtension("qbs.PropertyList");

function isBundleProduct(product) {
    return product.type.contains("applicationbundle")
        || product.type.contains("frameworkbundle")
        || product.type.contains("bundle")
        || product.type.contains("inapppurchase");
}

/**
  * Returns the package creator code for the given product based on its type.
  */
function packageType(product) {
    if (product.type.contains("application") || product.type.contains("applicationbundle"))
        return "APPL";
    else if (product.type.contains("frameworkbundle"))
        return "FMWK";
    else if (product.type.contains("bundle"))
        return "BNDL";

    throw ("Unsupported product type " + product.type + ". "
         + "Must be in {application, applicationbundle, frameworkbundle, bundle}.");
}

function infoPlistContents(infoPlistFilePath) {
    if (infoPlistFilePath === undefined)
        return undefined;

    var plist = new PropertyList();
    try {
        plist.readFromFile(infoPlistFilePath);
        return JSON.parse(plist.toJSONString());
    } finally {
        plist.clear();
    }
}

function infoPlistFormat(infoPlistFilePath) {
    if (infoPlistFilePath === undefined)
        return undefined;

    var plist = new PropertyList();
    try {
        plist.readFromFile(infoPlistFilePath);
        return plist.format();
    } finally {
        plist.clear();
    }
}

/**
  * Returns the main bundle directory.
  * @param version only used for framework bundles.
  * @note Xcode equivalent: CONTENTS_FOLDER_PATH
  */
function contentsFolderPath(product, version) {
    var path = wrapperName(product);

    if (product.type.contains("frameworkbundle"))
        path += "/Versions/" + (version || frameworkVersion(product));
    else if (!isShallowBundle(product))
        path += "/Contents";

    return path;
}

/**
  * Returns the directory for documentation files.
  * @param version only used for framework bundles.
  * @note Xcode equivalent: DOCUMENTATION_FOLDER_PATH
  */
function documentationFolderPath(product, localizationName, version) {
    var path = localizedResourcesFolderPath(product, localizationName, version);
    if (!product.type.contains("inapppurchase"))
        path += "/Documentation";
    return path;
}

/**
  * Returns the destination directory for auxiliary executables.
  * @param version only used for framework bundles.
  * @note Xcode equivalent: EXECUTABLES_FOLDER_PATH
  */
function executablesFolderPath(product, localizationName, version) {
    if (product.type.contains("frameworkbundle"))
        return localizedResourcesFolderPath(product, localizationName, version);
    else
        return _contentsFolderSubDirPath(product, "Executables", version);
}

/**
  * Returns the destination directory for the primary executable.
  * @param version only used for framework bundles.
  * @note Xcode equivalent: EXECUTABLE_FOLDER_PATH
  */
function executableFolderPath(product, version) {
    var path = contentsFolderPath(product, version);
    if (!isShallowBundle(product)
        && !product.type.contains("frameworkbundle")
        && !product.type.contains("inapppurchase"))
        path += "/MacOS";

    return path;
}

/**
  * Returns the path to the bundle's primary executable file.
  * @param version only used for framework bundles.
  * @note Xcode equivalent: EXECUTABLE_PATH
  */
function executablePath(product, version) {
    return executableFolderPath(product, version) + "/" + productName(product);
}

/**
  * Returns the major version number or letter corresponding to the bundle version.
  * @note Xcode equivalent: FRAMEWORK_VERSION
  */
function frameworkVersion(product) {
    if (!product.type.contains("frameworkbundle"))
        throw "Product type must be a frameworkbundle, was " + product.type;

    var n = parseInt(product.version, 10);
    return isNaN(n) ? 'A' : n;
}

/**
  * Returns the directory containing frameworks used by the bundle's executables.
  * @param version only used for framework bundles.
  * @note Xcode equivalent: FRAMEWORKS_FOLDER_PATH
  */
function frameworksFolderPath(product, version) {
    return _contentsFolderSubDirPath(product, "Frameworks", version);
}

/**
  * Returns the path to the bundle's main information property list.
  * @param version only used for framework bundles.
  * @note Xcode equivalent: INFOPLIST_PATH
  */
function infoPlistPath(product, version) {
    var path;
    if (product.type.contains("application"))
        path = ".tmp/" + product.name;
    else if (product.type.contains("frameworkbundle"))
        path = unlocalizedResourcesFolderPath(product, version);
    else if (product.type.contains("inapppurchase"))
        path = wrapperName(product);
    else
        path = contentsFolderPath(product, version);

    return path + "/" + _infoFileNames(product)[0];
}

/**
  * Returns the path to the strings file corresponding to the bundle's main information property list.
  * @param version only used for framework bundles.
  * @note Xcode equivalent: INFOSTRINGS_PATH
  */
function infoStringsPath(product, localizationName, version) {
    return localizedResourcesFolderPath(product, localizationName, version) + "/" + _infoFileNames(product)[1];
}

/**
  * Returns the path to the bundle's resources directory for the given localization.
  * @param version only used for framework bundles.
  * @note Xcode equivalent: LOCALIZED_RESOURCES_FOLDER_PATH
  */
function localizedResourcesFolderPath(product, localizationName, version) {
    if (typeof localizationName !== "string")
        throw("'" + localizationName + "' is not a valid localization name");

    return unlocalizedResourcesFolderPath(product, version) + "/" + localizationName + ".lproj";
}

/**
  * Returns the path to the bundle's PkgInfo (package info) file.
  * @note Xcode equivalent: PKGINFO_PATH
  */
function pkgInfoPath(product) {
    var path = (product.type.contains("frameworkbundle"))
        ? wrapperName(product)
        : contentsFolderPath(product);
    return path + "/PkgInfo";
}

/**
  * Returns the directory containing plugins used by the bundle's executables.
  * @param version only used for framework bundles.
  * @note Xcode equivalent: PLUGINS_FOLDER_PATH
  */
function pluginsFolderPath(product, version) {
    if (product.type.contains("frameworkbundle"))
        return unlocalizedResourcesFolderPath(product, version);

    return _contentsFolderSubDirPath(product, "PlugIns", version);
}

/**
  * Returns the directory containing private header files for the framework.
  * @param version only used for framework bundles.
  * @note Xcode equivalent: PRIVATE_HEADERS_FOLDER_PATH
  */
function privateHeadersFolderPath(product, version) {
    return _contentsFolderSubDirPath(product, "PrivateHeaders", version);
}

/**
  * Returns the name of the product (in Xcode terms) which corresponds to the target name in Qbs terms.
  * @note Xcode equivalent: PRODUCT_NAME
  */
function productName(product) {
    return product.targetName;
}

/**
  * Returns the directory containing public header files for the framework.
  * @param version only used for framework bundles.
  * @note Xcode equivalent: PUBLIC_HEADERS_FOLDER_PATH
  */
function publicHeadersFolderPath(product, version) {
    return _contentsFolderSubDirPath(product, "Headers", version);
}

/**
  * Returns the directory containing script files associated with the bundle.
  * @param version only used for framework bundles.
  * @note Xcode equivalent: SCRIPTS_FOLDER_PATH
  */
function scriptsFolderPath(product, version) {
    return unlocalizedResourcesFolderPath(product, version) + "/Scripts";
}

/**
  * Returns whether the bundle is a shallow bundle.
  * This controls the presence or absence of the Contents, MacOS and Resources folders.
  * iOS tends to store the majority of files in its bundles in the main directory.
  * @note Xcode equivalent: SHALLOW_BUNDLE
  */
function isShallowBundle(product) {
    return product.moduleProperty("qbs", "targetOS").contains("ios")
        && product.type.contains("applicationbundle");
}

/**
  * Returns the directory containing sub-frameworks that may be shared with other applications.
  * @param version only used for framework bundles.
  * @note Xcode equivalent: SHARED_FRAMEWORKS_FOLDER_PATH
  */
function sharedFrameworksFolderPath(product, version) {
    return _contentsFolderSubDirPath(product, "SharedFrameworks", version);
}

/**
  * Returns the directory containing supporting files that may be shared with other applications.
  * @param version only used for framework bundles.
  * @note Xcode equivalent: SHARED_SUPPORT_FOLDER_PATH
  */
function sharedSupportFolderPath(product, version) {
    if (product.type.contains("frameworkbundle"))
        return unlocalizedResourcesFolderPath(product, version);

    return _contentsFolderSubDirPath(product, "SharedSupport", version);
}

/**
  * Returns the directory containing resource files that are not specific to any given localization.
  * @note Xcode equivalent: UNLOCALIZED_RESOURCES_FOLDER_PATH
  */
function unlocalizedResourcesFolderPath(product, version) {
    if (isShallowBundle(product))
        return contentsFolderPath(product, version);

    return _contentsFolderSubDirPath(product, "Resources", version);
}

/**
  * Returns the path to the bundle's version.plist file.
  * @param version only used for framework bundles.
  * @note Xcode equivalent: VERSIONPLIST_PATH
  */
function versionPlistPath(product, version) {
    var path = (product.type.contains("frameworkbundle"))
        ? unlocalizedResourcesFolderPath(product, version)
        : contentsFolderPath(product, version);
    return path + "/version.plist";
}

/**
  * Returns the file extension of the bundle directory - app, framework, bundle, etc.
  * @note Xcode equivalent: WRAPPER_EXTENSION
  */
function wrapperExtension(product) {
    if (product.type.contains("applicationbundle")) {
        return "app";
    } else if (product.type.contains("frameworkbundle")) {
        return "framework";
    } else if (product.type.contains("inapppurchase")) {
        return "";
    } else if (product.type.contains("bundle")) {
        // Potentially: kext, prefPane, qlgenerator, saver, mdimporter, or a custom extension
        var bundleExtension = ModUtils.moduleProperty(product, "bundleExtension");

        // default to bundle if none was specified by the user
        return bundleExtension || "bundle";
    } else {
        throw ("Unsupported bundle product type " + product.type + ". "
             + "Must be in {applicationbundle, frameworkbundle, bundle, inapppurchase}.");
    }
}

/**
  * Returns the name of the bundle directory - the product name plus the bundle extension.
  * @note Xcode equivalent: WRAPPER_NAME
  */
function wrapperName(product) {
    return productName(product) + wrapperSuffix(product);
}

/**
  * Returns the suffix of the bundle directory, that is, its extension prefixed with a '.',
  * or an empty string if the extension is also an empty string.
  * @note Xcode equivalent: WRAPPER_SUFFIX
  */
function wrapperSuffix(product) {
    var ext = wrapperExtension(product);
    return ext ? ("." + ext) : "";
}

// Private helper functions

/**
  * In-App purchase content bundles use virtually no subfolders of Contents;
  * this is a convenience method to avoid repeating that logic everywhere.
  * @param version only used for framework bundles.
  */
function _contentsFolderSubDirPath(product, subdirectoryName, version) {
    var path = contentsFolderPath(product, version);
    if (!product.type.contains("inapppurchase"))
        path += "/" + subdirectoryName;
    return path;
}

/**
  * Returns a list containing the filename of the bundle's main information
  * property list and filename of the corresponding strings file.
  */
function _infoFileNames(product) {
    if (product.type.contains("inapppurchase"))
        return ["ContentInfo.plist", "ContentInfo.strings"];
    else
        return ["Info.plist", "InfoPlist.strings"];
}
