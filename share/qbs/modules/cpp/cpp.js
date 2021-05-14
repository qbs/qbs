/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
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
var PathTools = require("qbs.PathTools");
var Utilities = require("qbs.Utilities");

function languageVersion(versionArray, knownValues, lang) {
    if (!versionArray)
        return undefined;
    var versions = [].uniqueConcat(versionArray);
    if (versions.length === 1)
        return versions[0];
    for (var i = 0; i < knownValues.length; ++i) {
        var candidate = knownValues[i];
        if (versions.contains(candidate))
            return candidate;
    }
    var version = versions[0];
    console.debug("Randomly choosing '" + version
                  + "' from list of unknown " + lang + " version strings (" + versions + ")");
    return version;
}

function extractMacros(output) {
    var m = {};
    output.trim().split(/\r?\n/g).map(function (line) {
        var prefix = "#define ";
        if (!line.startsWith(prefix))
            return;
        var index = line.indexOf(" ", prefix.length);
        if (index !== -1)
            m[line.substr(prefix.length, index - prefix.length)] = line.substr(index + 1);
    });
    return m;
}

function compilerOutputTags(needsListingFiles) {
    var tags = ["obj"];
    if (needsListingFiles)
        tags.push("lst");
    return tags;
}

function applicationLinkerOutputTags(needsLinkerMapFile) {
    var tags = ["application"];
    if (needsLinkerMapFile)
        tags.push("mem_map");
    return tags;
}

function staticLibraryLinkerOutputTags() {
    return ["staticlibrary"];
}

function compilerOutputArtifacts(input, isCompilerArtifacts) {
    var artifacts = [];
    artifacts.push({
        fileTags: ["obj"],
        filePath: Utilities.getHash(input.baseDir) + "/"
              + input.fileName + input.cpp.objectSuffix
    });
    if (isCompilerArtifacts && input.cpp.generateCompilerListingFiles) {
        artifacts.push({
            fileTags: ["lst"],
            filePath: Utilities.getHash(input.baseDir) + "/"
              + input.fileName + input.cpp.compilerListingSuffix
        });
    } else if (!isCompilerArtifacts && input.cpp.generateAssemblerListingFiles) {
        artifacts.push({
            fileTags: ["lst"],
            filePath: Utilities.getHash(input.baseDir) + "/"
              + input.fileName + input.cpp.assemblerListingSuffix
        });
    }
    return artifacts;
}

function applicationLinkerOutputArtifacts(product) {
    var artifacts = [{
        fileTags: ["application"],
        filePath: FileInfo.joinPaths(
            product.destinationDirectory,
            PathTools.applicationFilePath(product))
    }];
    if (product.cpp.generateLinkerMapFile) {
        artifacts.push({
            fileTags: ["mem_map"],
            filePath: FileInfo.joinPaths(
                product.destinationDirectory,
                product.targetName + product.cpp.linkerMapSuffix)
        });
    }
    return artifacts;
}

function staticLibraryLinkerOutputArtifacts(product) {
    var staticLib = {
        fileTags: ["staticlibrary"],
        filePath: FileInfo.joinPaths(
                      product.destinationDirectory,
                      PathTools.staticLibraryFilePath(product))
    };
    return [staticLib]
}

function collectLibraryDependencies(product) {
    var seen = {};
    var result = [];

    function addFilePath(filePath) {
        result.push({ filePath: filePath });
    }

    function addArtifactFilePaths(dep, artifacts) {
        if (!artifacts)
            return;
        var artifactFilePaths = artifacts.map(function(a) { return a.filePath; });
        artifactFilePaths.forEach(addFilePath);
    }

    function addExternalStaticLibs(obj) {
        if (!obj.cpp)
            return;
        function ensureArray(a) {
            return (a instanceof Array) ? a : [];
        }
        function sanitizedModuleListProperty(obj, moduleName, propertyName) {
            return ensureArray(ModUtils.sanitizedModuleProperty(obj, moduleName, propertyName));
        }
        var externalLibs = [].concat(
                    sanitizedModuleListProperty(obj, "cpp", "staticLibraries"));
        var staticLibrarySuffix = obj.moduleProperty("cpp", "staticLibrarySuffix");
        externalLibs.forEach(function(staticLibraryName) {
            if (!staticLibraryName.endsWith(staticLibrarySuffix))
                staticLibraryName += staticLibrarySuffix;
            addFilePath(staticLibraryName);
        });
    }

    function traverse(dep) {
        if (seen.hasOwnProperty(dep.name))
            return;
        seen[dep.name] = true;

        if (dep.parameters.cpp && dep.parameters.cpp.link === false)
            return;

        var staticLibraryArtifacts = dep.artifacts["staticlibrary"];
        if (staticLibraryArtifacts) {
            dep.dependencies.forEach(traverse);
            addArtifactFilePaths(dep, staticLibraryArtifacts);
            addExternalStaticLibs(dep);
        }
    }

    product.dependencies.forEach(traverse);
    addExternalStaticLibs(product);
    return result;
}
