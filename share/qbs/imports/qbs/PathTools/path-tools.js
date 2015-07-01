/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

var FileInfo = loadExtension("qbs.FileInfo");

function applicationFileName(product) {
    return product.moduleProperty("cpp", "executablePrefix")
         + product.targetName
         + product.moduleProperty("cpp", "executableSuffix");
}

function applicationFilePath(product) {
    if (product.moduleProperty("bundle", "isBundle"))
        return product.moduleProperty("bundle", "executablePath");
    else
        return applicationFileName(product);
}

function loadableModuleFileName(product) {
    return product.moduleProperty("cpp", "loadableModulePrefix")
         + product.targetName
         + product.moduleProperty("cpp", "loadableModuleSuffix");
}

function loadableModuleFilePath(product) {
    if (product.moduleProperty("bundle", "isBundle"))
        return product.moduleProperty("bundle", "executablePath");
    else
        return loadableModuleFileName(product);
}

function staticLibraryFileName(product) {
    return product.moduleProperty("cpp", "staticLibraryPrefix")
         + product.targetName
         + product.moduleProperty("cpp", "staticLibrarySuffix");
}

function staticLibraryFilePath(product) {
    if (product.moduleProperty("bundle", "isBundle"))
        return product.moduleProperty("bundle", "executablePath");
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
    if (product.moduleProperty("bundle", "isBundle"))
        return product.moduleProperty("bundle", "executablePath");
    else
        return dynamicLibraryFileName(product, version, maxParts);
}

function importLibraryFilePath(product) {
    return product.moduleProperty("cpp", "dynamicLibraryPrefix")
         + product.targetName
         + product.moduleProperty("cpp", "dynamicLibraryImportSuffix");
}

function debugInfoFileName(product) {
    if (product.moduleProperty("bundle", "isBundle"))
        return product.moduleProperty("bundle", "bundleName") + product.moduleProperty("cpp", "debugInfoSuffix");
    else if (product.type.contains("application"))
        return applicationFileName(product) + product.moduleProperty("cpp", "debugInfoSuffix");
    else if (product.type.contains("dynamiclibrary"))
        return dynamicLibraryFileName(product) + product.moduleProperty("cpp", "debugInfoSuffix");
    else if (product.type.contains("loadablemodule"))
        return loadableModuleFileName(product) + product.moduleProperty("cpp", "debugInfoSuffix");
    else if (product.type.contains("staticlibrary"))
        return staticLibraryFileName(product) + product.moduleProperty("cpp", "debugInfoSuffix");
    else
        return product.targetName + product.moduleProperty("cpp", "debugInfoSuffix");
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

function frameworkExecutablePath(frameworkPath) {
    var suffix = ".framework";
    var isAbsoluteFrameworkPath = frameworkPath.slice(-suffix.length) === suffix;
    if (isAbsoluteFrameworkPath) {
        var frameworkName = FileInfo.fileName(frameworkPath).slice(0, -suffix.length);
        return FileInfo.joinPaths(frameworkPath, frameworkName);
    }
    return undefined;
}

// pathList is also a string, using the respective separator
function prependOrSetPath(path, pathList, separator) {
    if (!pathList || pathList.length === 0)
        return path;
    return path + separator + pathList;
}
