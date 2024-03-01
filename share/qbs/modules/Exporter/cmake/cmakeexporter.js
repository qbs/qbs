/****************************************************************************
**
** Copyright (C) 2024 Raphaël Cotty <raphael.cotty@gmail.com>
** Copyright (C) 2024 Ivan Komissarov (abbapoh@gmail.com)
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qbs.
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

var FileInfo = require("qbs.FileInfo");
var ModUtils = require("qbs.ModUtils");
var ExporterHelpers = require("../exporter.js");

function tagListToString(tagList)
{
    return JSON.stringify(tagList);
}

function collectAutodetectedData(project, topLevelProduct, outputs)
{
    var packageName = topLevelProduct.Exporter.cmake.packageName;

    var data = {};
    data.packageName = packageName;
    data.installPrefixDir = "_" + packageName.toUpperCase() + "_INSTALL_PREFIX";
    data.packages = [];

    function quote(value)
    {
        return "\"" + value + "\"";
    }

    function quoteAndPrefixify(propName)
    {
        function quoteAndPrefixifyHelper(value) {
            var prefixToStrip =
                ExporterHelpers.getPrefixToStrip(project, topLevelProduct, propName, value);
            if (typeof value !== "string"
                    || !prefixToStrip
                    || (value.length > prefixToStrip.length
                        && value[prefixToStrip.length] !== '/')) {
                return quote(value);
            }
            return quote("${" + data.installPrefixDir + "}" + value.slice(prefixToStrip.length));
        }
        return quoteAndPrefixifyHelper;
    }

    var installedOutputFilePath = ModUtils.artifactInstalledFilePath(
        outputs["Exporter.cmake.configFile"][0]);
    var installedOutputPathName = FileInfo.path(installedOutputFilePath);

    var installRootPath = FileInfo.joinPaths(topLevelProduct.qbs.installRoot, topLevelProduct.qbs.installPrefix);
    data.installPrefix = FileInfo.relativePath(installedOutputPathName, installRootPath);

    var libArtifacts;
    var libImportArtifacts;
    var isProduct = !topLevelProduct.present;
    var considerFramework = !isProduct || (topLevelProduct.type
        && topLevelProduct.type.includes("bundle.content"))
        && topLevelProduct.bundle
        && topLevelProduct.bundle.isBundle
        && topLevelProduct.qbs.targetOS.includes("darwin");
    var considerDynamicLibs = !isProduct || (topLevelProduct.type
        && topLevelProduct.type.includes("dynamiclibrary"));
    var considerStaticLibs = !isProduct || (topLevelProduct.type
        && topLevelProduct.type.includes("staticlibrary"));
    if (considerFramework) {
        libArtifacts = topLevelProduct.artifacts["bundle.symlink.executable"];
        if (considerDynamicLibs)
            data.type = "SHARED";
        else if (considerStaticLibs)
            data.type = "STATIC";
        else
            data.type = "INTERFACE";
    } else if (considerDynamicLibs) {
        libArtifacts = topLevelProduct.artifacts.dynamiclibrary;
        libImportArtifacts = topLevelProduct.artifacts.dynamiclibrary_import;
        data.type = "SHARED";
    } else if (considerStaticLibs) {
        libArtifacts = topLevelProduct.artifacts.staticlibrary;
        data.type = "STATIC";
    } else {
        data.type = "INTERFACE";
    }

    for (var i = 0; i < (libArtifacts || []).length; ++i) {
        var libArtifact = libArtifacts[i];
        var libImportArtifact = (libImportArtifacts || [])[i];
        if (libArtifact.qbs.install) {
            var installPath = ModUtils.artifactInstalledFilePath(libArtifact);
            data.importedLocation = quoteAndPrefixify("installRoot")(installPath);
            data.soName = topLevelProduct.targetName;
            if (libImportArtifact && libImportArtifact.qbs.install) {
                installPath = ModUtils.artifactInstalledFilePath(libImportArtifact);
                data.importedImplibLocation = quoteAndPrefixify("installRoot")(installPath);
            }
            break;
        }
    }
    var cpp = topLevelProduct.exports.cpp;
    if (cpp) {
        data.libraryPaths = (cpp.libraryPaths || []).map(quoteAndPrefixify("cpp.libraryPaths"));

        data.linkLibraries = [];
        data.linkLibraries = data.linkLibraries.concat(cpp.dynamicLibraries || []);
        data.linkLibraries = data.linkLibraries.concat(cpp.staticLibraries || []);
        data.linkLibraries = data.linkLibraries.map(quoteAndPrefixify("cpp.dynamicLibraries"));

        data.linkOptions = [];
        data.linkOptions = data.linkOptions.concat(cpp.driverLinkerFlags || []);
        if ((cpp.linkerFlags || []).length > 0) {
            data.linkOptions =
                data.linkOptions.concat("LINKER:" + (cpp.linkerFlags || []).join(","));
        }
        data.linkOptions = data.linkOptions.map(quote);

        data.includeDirectories =
            (cpp.includePaths || []).map(quoteAndPrefixify("cpp.includePaths"));
        data.compileDefinitions = (cpp.defines || []).map(quote);

        data.compileOptions = [];
        data.compileOptions = data.compileOptions.concat(cpp.commonCompilerFlags || []);
        data.compileOptions = data.compileOptions.concat(cpp.driverFlags || []);
        data.compileOptions = data.compileOptions.concat(cpp.cxxFlags || []);
        data.compileOptions = data.compileOptions.concat(cpp.cFlags || []);
        data.compileOptions = data.compileOptions.map(quote);
    }

    function gatherDeps(dep) {
        if (dep.name === "Exporter.cmake")
            return;
        var depHasExporter = dep.Exporter && dep.Exporter.cmake;
        if (!depHasExporter)
            return;
        data.packages.push(dep.Exporter.cmake.packageName);
    }

    var exportedDeps = topLevelProduct.exports ? topLevelProduct.exports.dependencies : [];
    exportedDeps.forEach(gatherDeps);

    return data;
}

function writeConfigFile(project, product, outputs)
{
    var autoDetectedData = collectAutodetectedData(project, product, outputs);
    var packageName = autoDetectedData.packageName;

    function writeCommand(command, lines)
    {
        if ((lines || []).length === 0)
            return;
        cmakeConfigFile.writeLine(command + "(" + packageName + " INTERFACE");
        for (i = 0; i < lines.length; i++) {
            cmakeConfigFile.writeLine("  " + lines[i]);
        }
        cmakeConfigFile.writeLine(")");
    }

    var cmakeConfigFile = new TextFile(outputs["Exporter.cmake.configFile"][0].filePath,
                                       TextFile.WriteOnly);
    cmakeConfigFile.writeLine("# Generated by Qbs");

    cmakeConfigFile.writeLine("cmake_minimum_required(VERSION 3.5)");

    cmakeConfigFile.writeLine("if(TARGET " + packageName + ")");
    cmakeConfigFile.writeLine("  return()");
    cmakeConfigFile.writeLine("endif()");

    cmakeConfigFile.writeLine("set(" + autoDetectedData.installPrefixDir +
                              " \"${CMAKE_CURRENT_LIST_DIR}/" +
                              autoDetectedData.installPrefix + "\")");

    autoDetectedData.packages.forEach(function(packageName) {
        cmakeConfigFile.writeLine("find_package(" + packageName + " REQUIRED SILENT)");
    });
    cmakeConfigFile.writeLine(
        "add_library(" + packageName + " " + autoDetectedData.type + " IMPORTED)");
    var configuration = (product.qbs.buildVariant) ?
                product.qbs.buildVariant.toUpperCase() : "NONE";
    cmakeConfigFile.writeLine("set_property(TARGET " + packageName +
                              " APPEND PROPERTY IMPORTED_CONFIGURATIONS " +
                              configuration + ")");

    cmakeConfigFile.writeLine("set_target_properties(" + packageName + " PROPERTIES");
    cmakeConfigFile.writeLine("  IMPORTED_LINK_INTERFACE_LANGUAGES_" + configuration +
                              " CXX");
    if (autoDetectedData.type !== "INTERFACE") {
        cmakeConfigFile.writeLine("  IMPORTED_LOCATION_" + configuration + " " +
                                autoDetectedData.importedLocation);
    }
    if (autoDetectedData.importedImplibLocation) {
        cmakeConfigFile.writeLine("  IMPORTED_IMPLIB_" + configuration + " " +
                                  autoDetectedData.importedImplibLocation);
    }
    cmakeConfigFile.writeLine(")");

    writeCommand("target_link_directories", autoDetectedData.libraryPaths);
    writeCommand("target_link_libraries",
        autoDetectedData.linkLibraries.concat(autoDetectedData.packages));
    writeCommand("target_link_options", autoDetectedData.linkOptions);
    writeCommand("target_include_directories", autoDetectedData.includeDirectories);
    writeCommand("target_compile_definitions", autoDetectedData.compileDefinitions);
    writeCommand("target_compile_options", autoDetectedData.compileOptions);

    cmakeConfigFile.close();
}

function writeVersionFile(product, outputs)
{
    var cmakeVersionFile = new TextFile(
        outputs["Exporter.cmake.versionFile"][0].filePath, TextFile.WriteOnly);
    cmakeVersionFile.writeLine("# Generated by Qbs");
    cmakeVersionFile.writeLine("set(PACKAGE_VERSION \"" + product.version + "\")");
    cmakeVersionFile.close();
}
