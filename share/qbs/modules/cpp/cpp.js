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

var BinaryFile = require("qbs.BinaryFile");
var File = require("qbs.File");
var FileInfo = require("qbs.FileInfo");
var ModUtils = require("qbs.ModUtils");
var PathTools = require("qbs.PathTools");
var TextFile = require("qbs.TextFile");
var Utilities = require("qbs.Utilities");

function languageVersion(versionArray, knownValues, lang) {
    if (!versionArray)
        return undefined;
    var versions = [].uniqueConcat(versionArray);
    if (versions.length === 1)
        return versions[0];
    for (var i = 0; i < knownValues.length; ++i) {
        var candidate = knownValues[i];
        if (versions.includes(candidate))
            return candidate;
    }
    var version = versions[0];
    console.debug("Randomly choosing '" + version
                  + "' from list of unknown " + lang + " version strings (" + versions + ")");
    return version;
}

function extractMacros(output, regexp) {
    var m = {};
    output.trim().split(/\r?\n/g).map(function(line) {
        if (regexp) {
            var match = regexp.exec(line);
            if (!match)
                return;
            line = match[1];
        }
        var prefix = "#define ";
        if (!line.startsWith(prefix))
            return;
        var index = line.indexOf(" ", prefix.length);
        if (index !== -1)
            m[line.substr(prefix.length, index - prefix.length)] = line.substr(index + 1);
    });
    return m;
}

function relativePath(baseDirectory, filePath) {
    if (FileInfo.isAbsolutePath(filePath))
        return FileInfo.relativePath(baseDirectory, filePath);
    return filePath;
}

function assemblerOutputTags(needsListingFiles) {
    var tags = ["obj"];
    if (needsListingFiles)
        tags.push("lst");
    return tags;
}

function compilerOutputTags(withListingFiles, withCxxModules) {
    var tags = ["obj", "intermediate_obj"];
    if (withListingFiles)
        tags.push("lst");
    if (withCxxModules)
        tags = tags.concat(["compiled-module", "modulemap", "moduleinfo"]);
    return tags;
}

function applicationLinkerOutputTags(needsLinkerMapFile) {
    var tags = ["application"];
    if (needsLinkerMapFile)
        tags.push("mem_map");
    return tags;
}

function dynamicLibraryLinkerOutputTags() {
    return ["dynamiclibrary", "dynamiclibrary_import"];
}

function staticLibraryLinkerOutputTags() {
    return ["staticlibrary"];
}

function resourceCompilerOutputTags() {
    return ["res"];
}

function precompiledHeaderOutputTags(lang, generateObjects) {
    var tags = [lang + "_pch"];
    if (generateObjects)
        tags.push("obj");
    return tags;
};

function assemblerOutputArtifacts(input) {
    var artifacts = [];
    artifacts.push({
        fileTags: ["obj"],
        filePath: FileInfo.joinPaths(Utilities.getHash(input.baseDir),
                                     input.fileName + input.cpp.objectSuffix)
    });
    if (input.cpp.generateAssemblerListingFiles) {
        artifacts.push({
            fileTags: ["lst"],
            filePath: FileInfo.joinPaths(Utilities.getHash(input.baseDir),
                                         input.fileName + input.cpp.assemblerListingSuffix)
        });
    }
    return artifacts;
}

function cxxModulesArtifacts(input) {
    var artifacts = [];
    if (input.cpp.forceUseCxxModules === false)
        return artifacts;

    if (input.cpp.compiledModuleSuffix === undefined
        || input.cpp.moduleOutputFlag === undefined
        || input.cpp.moduleFileFlag === undefined) {
        console.warn("Skip additional artifacts for '" + input.fileName + "' since '"
            + input.qbs.toolchainType + "' toolchain does not support C++ Modules.");
        return artifacts;
    }

    var moduleInformation = CppModulesScanner.apply(input);

    if (moduleInformation.providesModule || moduleInformation.requiresModules.length > 0) {
        artifacts.push({
            filePath: FileInfo.joinPaths(Utilities.getHash(input.baseDir),
                                         input.fileName + input.cpp.moduleMapSuffix),
            fileTags: ["modulemap"],
            // we can't use maps here only strings and lists
            cpp: {
                _providesModule: moduleInformation.providesModule,
                _isInterfaceModule: moduleInformation.isInterfaceModule,
                _requiresModules : moduleInformation.requiresModules,
            },
        })
    }
    if (input.fileTags.includes("cppm")) {
        const moduleName = moduleInformation.providesModule
            ? moduleInformation.providesModule : undefined;
        if (moduleName) {
            const moduleFileName = moduleName.replace(':', '-');
            artifacts.push({
                filePath: FileInfo.joinPaths("cxx-modules", moduleFileName + product.cpp.compiledModuleSuffix),
                fileTags: ["compiled-module"],
            })
            artifacts.push({
                filePath: FileInfo.joinPaths("cxx-modules", moduleFileName + product.cpp.moduleInfoSuffix),
                fileTags: ["moduleinfo"],
            })
        } else {
            console.warn("File '" + input.fileName + "' has 'cppm' tag, but does not "
                + "provide a module interface unit or partition");
        }
    }
    return artifacts;
}

function compilerOutputArtifacts(input, inputs, withCxxModules) {
    var objTags = input.fileTags.includes("cpp_intermediate_object")
        ? ["intermediate_obj"]
        : ["obj"];
    if (inputs) {
        if (inputs.c || inputs.objc)
            objTags.push("c_obj");
        if (inputs.cpp || inputs.objcpp || inputs.cppm)
            objTags.push("cpp_obj");
    }
    var artifacts = [{
        fileTags: objTags,
        filePath: FileInfo.joinPaths(Utilities.getHash(input.baseDir),
                                     input.fileName + input.cpp.objectSuffix)
    }];
    if (input.cpp.generateCompilerListingFiles) {
        artifacts.push({
            fileTags: ["lst"],
            filePath: FileInfo.joinPaths(Utilities.getHash(input.baseDir),
                                         input.fileName + input.cpp.compilerListingSuffix)
        });
    }
    if (withCxxModules)
        artifacts = artifacts.concat(cxxModulesArtifacts(input));
    return artifacts;
}

function applicationLinkerOutputArtifacts(product) {
    var artifacts = [{
        fileTags: ["application"],
        filePath: FileInfo.joinPaths(product.destinationDirectory,
                                     PathTools.applicationFilePath(product))
    }];
    if (product.cpp.generateLinkerMapFile) {
        artifacts.push({
            fileTags: ["mem_map"],
            filePath: FileInfo.joinPaths(product.destinationDirectory,
                                         product.targetName + product.cpp.linkerMapSuffix)
        });
    }
    return artifacts;
}

function dynamicLibraryLinkerOutputArtifacts(product) {
    return [{
        fileTags: ["dynamiclibrary"],
        filePath: FileInfo.joinPaths(product.destinationDirectory,
                                     PathTools.dynamicLibraryFilePath(product))
    }, {
        fileTags: ["dynamiclibrary_import"],
        filePath: FileInfo.joinPaths(product.destinationDirectory,
                                     PathTools.importLibraryFilePath(product)),
        alwaysUpdated: false
    }];
}

function staticLibraryLinkerOutputArtifacts(product) {
    return [{
        fileTags: ["staticlibrary"],
        filePath: FileInfo.joinPaths(product.destinationDirectory,
                                     PathTools.staticLibraryFilePath(product))
    }];
}

function resourceCompilerOutputArtifacts(input) {
    return [{
        fileTags: ["res"],
        filePath: FileInfo.joinPaths(Utilities.getHash(input.baseDir),
                                     input.completeBaseName + input.cpp.resourceSuffix)
    }];
}

function precompiledHeaderOutputArtifacts(input, product, lang, generateObjects) {
    var artifacts = [{
        fileTags: [lang + "_pch"],
        filePath: product.name + "_" + lang + product.cpp.precompiledHeaderSuffix
    }];
    if (generateObjects) {
        artifacts.push({
            fileTags: ["obj"],
            filePath: Utilities.getHash(input.completeBaseName)
                        + "_" + lang + input.cpp.objectSuffix
        });
    }
    return artifacts;
}

function collectLibraryDependencies(product) {
    var seen = {};
    var seenObjectFiles = [];
    var result = [];

    function addFilePath(filePath, wholeArchive, productName) {
        result.push({ filePath: filePath, wholeArchive: wholeArchive, productName: productName });
    }

    function addArtifactFilePaths(dep, artifacts) {
        if (!artifacts)
            return;
        var artifactFilePaths = artifacts.map(function(a) { return a.filePath; });
        var wholeArchive = dep.parameters.cpp && dep.parameters.cpp.linkWholeArchive;
        var artifactsAreImportLibs = artifacts.length > 0
                && artifacts[0].fileTags.includes("dynamiclibrary_import");
        for (var i = 0; i < artifactFilePaths.length; ++i) {
            addFilePath(artifactFilePaths[i], wholeArchive,
                        artifactsAreImportLibs ? dep.name : undefined);
        }
    }

    function addExternalLibs(obj) {
        if (!obj.cpp)
            return;
        function ensureArray(a) {
            return (a instanceof Array) ? a : [];
        }
        function sanitizedModuleListProperty(obj, moduleName, propertyName) {
            return ensureArray(ModUtils.sanitizedModuleProperty(obj, moduleName, propertyName));
        }
        function handleExternalLibraries(tag, libSuffix, objSuffix) {
            var externalLibs = sanitizedModuleListProperty(obj, "cpp", tag) || [];
            externalLibs.forEach(function(libName) {
                var isObjectFile = objSuffix && libName.endsWith(objSuffix);
                if (isObjectFile) {
                    if (seenObjectFiles.includes(libName))
                        return;
                    seenObjectFiles.push(libName);
                }
                if (!libName.endsWith(libSuffix) && !isObjectFile && !libName.startsWith('@'))
                    libName += libSuffix;
                addFilePath(libName, false);
            });
        }
        handleExternalLibraries("staticLibraries",
                                obj.moduleProperty("cpp", "staticLibrarySuffix"),
                                obj.moduleProperty("cpp", "objectSuffix"));
        handleExternalLibraries("dynamicLibraries",
                                obj.moduleProperty("cpp", "dynamicLibraryImportSuffix"));
    }

    function traverse(dep) {
        if (seen.hasOwnProperty(dep.name))
            return;
        if (dep.parameters.cpp && dep.parameters.cpp.link === false)
            return;

        var staticLibraryArtifacts = dep.artifacts["staticlibrary"];
        var dynamicLibraryArtifacts = staticLibraryArtifacts
                ? null : dep.artifacts["dynamiclibrary_import"];
        if (staticLibraryArtifacts) {
            seen[dep.name] = true;
            dep.dependencies.forEach(traverse);
            addArtifactFilePaths(dep, staticLibraryArtifacts);
            if (product.cpp.importPrivateLibraries)
                addExternalLibs(dep);
        } else if (dynamicLibraryArtifacts) {
            seen[dep.name] = true;
            addArtifactFilePaths(dep, dynamicLibraryArtifacts);
        }
    }

    product.dependencies.forEach(traverse);
    addExternalLibs(product);
    return result;
}

function collectAbsoluteLibraryDependencyPaths(product) {
    var paths = collectLibraryPaths(product);
    var deps = collectLibraryDependencies(product);
    return deps.map(function(dep) {
        var filePath = dep.filePath;
        if (FileInfo.isAbsolutePath(filePath))
            return filePath;
        for (var i = 0; i < paths.length; ++i) {
            var fullPath = FileInfo.joinPaths(paths[i], filePath);
            if (File.exists(fullPath))
                return fullPath;
        }
        return filePath;
    });
}

function collectDefines(input) {
    var allDefines = [];
    var platformDefines = input.cpp.platformDefines;
    if (platformDefines)
        allDefines = allDefines.uniqueConcat(platformDefines);
    var defines = input.cpp.defines;
    if (defines)
        allDefines = allDefines.uniqueConcat(defines);
    return allDefines;
}

function collectIncludePaths(input) {
    var allIncludePaths = [];
    var includePaths = input.cpp.includePaths;
    if (includePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(includePaths);
    var builtIns = input.cpp.compilerIncludePaths;
    return allIncludePaths.filter(function(p) {
        return !builtIns.includes(p);
    });
}

function collectSystemIncludePaths(input) {
    var allIncludePaths = [];
    var systemIncludePaths = input.cpp.systemIncludePaths;
    if (systemIncludePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(systemIncludePaths);
    var distributionIncludePaths = input.cpp.distributionIncludePaths;
    if (distributionIncludePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(distributionIncludePaths);
    var builtIns = input.cpp.compilerIncludePaths;
    return allIncludePaths.filter(function(p) {
        return !builtIns.includes(p);
    });
}

function collectPreincludePaths(input) {
    return input.cpp.prefixHeaders;
}

function collectLibraryPaths(product) {
    var allLibraryPaths = [];
    var libraryPaths = product.cpp.libraryPaths;
    if (libraryPaths)
        allLibraryPaths = allLibraryPaths.uniqueConcat(libraryPaths);
    var distributionLibraryPaths = product.cpp.distributionLibraryPaths;
    if (distributionLibraryPaths)
        allLibraryPaths = allLibraryPaths.uniqueConcat(distributionLibraryPaths);
    return allLibraryPaths;
}

function collectLinkerScriptPaths(inputs) {
    return inputs.linkerscript
            ? inputs.linkerscript.map(function(script) { return script.filePath; })
            : [];
}

function collectLinkerObjectPaths(inputs) {
    return inputs.obj ? inputs.obj.map(function(obj) { return obj.filePath; }) : [];
}

function collectResourceObjectPaths(inputs) {
    return inputs.res ? inputs.res.map(function(res) { return res.filePath; }) : [];
}

function collectLibraryDependenciesArguments(product) {
    var deps = collectLibraryDependencies(product);
    return deps.map(function(dep) { return product.cpp.libraryDependencyFlag + dep.filePath })
}

function collectDefinesArguments(input) {
    var defines = collectDefines(input);
    return defines.map(function(define) { return input.cpp.defineFlag + define });
}

function collectIncludePathsArguments(input) {
    var paths = collectIncludePaths(input);
    return paths.map(function(path) { return input.cpp.includeFlag + path });
}

function collectSystemIncludePathsArguments(input, flag) {
    flag = (flag === undefined) ? input.cpp.systemIncludeFlag : flag;
    var paths = collectSystemIncludePaths(input);
    return paths.map(function(path) { return flag + path });
}

function collectPreincludePathsArguments(input, split) {
    var paths = collectPreincludePaths(input);
    if (split) {
        var args = [];
        for (var i = 0; i < paths.length; ++i)
            args.push(input.cpp.preincludeFlag, paths[i]);
        return args;
    } else {
        return paths.map(function(path) { return input.cpp.preincludeFlag + path });
    }
}

function collectLibraryPathsArguments(product, flag) {
    flag = (flag === undefined) ? product.cpp.libraryPathFlag : flag;
    var paths = collectLibraryPaths(product);
    return paths.map(function(path) { return flag + path });
}

function collectLinkerScriptPathsArguments(product, inputs, split, flag) {
    flag = (flag === undefined) ? product.cpp.linkerScriptFlag : flag;
    var paths = collectLinkerScriptPaths(inputs);
    if (split) {
        var args = [];
        for (var i = 0; i < paths.length; ++i)
            args.push(flag, paths[i]);
        return args;
    } else {
        return paths.map(function(path) { return flag + path });
    }
}

function collectLinkerObjectPathsArguments(product, inputs, flag) {
    flag = (flag === undefined) ? "" : flag;
    var paths = collectLinkerObjectPaths(product);
    return paths.map(function(path) { return flag + path });
}

function collectMiscCompilerArguments(input, tag) {
    if (tag === "cppm")
        tag = "cpp";
    return [].concat(ModUtils.moduleProperty(input, "platformFlags"),
                     ModUtils.moduleProperty(input, "flags"),
                     ModUtils.moduleProperty(input, "platformFlags", tag),
                     ModUtils.moduleProperty(input, "flags", tag));
}

function collectMiscAssemblerArguments(input, tag) {
    return [].concat(ModUtils.moduleProperty(input, "platformFlags", tag),
                     ModUtils.moduleProperty(input, "flags", tag));
}

function collectMiscDriverArguments(config) {
    return [].concat(ModUtils.moduleProperty(config, "platformDriverFlags"),
                     ModUtils.moduleProperty(config, "driverFlags"),
                     ModUtils.moduleProperty(config, "targetDriverFlags"));
}

function collectMiscLinkerArguments(product) {
    return [].concat(ModUtils.moduleProperty(product, "driverLinkerFlags"));
}

function collectMiscEscapableLinkerArguments(product) {
    return [].concat(ModUtils.moduleProperty(product, "platformLinkerFlags"),
                     ModUtils.moduleProperty(product, "linkerFlags"));
}

function supportsArchitecture(architecture, knownArchitectures) {
    for (var i = 0; i < knownArchitectures.length; ++i) {
        if (architecture.startsWith("arm")) {
            if (architecture.startsWith(knownArchitectures[i]))
                return true;
        } else {
            if (architecture === knownArchitectures[i])
                return true;
        }
    }
    return false;
}

function prepareModuleInfo(moduleInformation, input, output, product) {
    var command = new JavaScriptCommand()
    command.moduleInformation = moduleInformation
    command.outPath = output.filePath
    command.description = "generating module info for " + input.fileName
    command.highlight = "filegen"

    command.sourceCode = function() {
        var file = new TextFile(outPath, TextFile.WriteOnly);
        file.write(JSON.stringify(moduleInformation, undefined, 4));
        file.close();
    }

    return command;
}

function prepareModuleMap(moduleInformation, input, mm, product) {
    var modulesFromDeps = {};

    product.dependencies.forEach(function(dep){
        (dep.artifacts["compiled-module"] || []).forEach(function(a) {
            modulesFromDeps[a.completeBaseName.replace('-', ':')] = a.filePath;
        });
    });

    var generateModuleMap = new JavaScriptCommand()
    generateModuleMap.outPath = mm.filePath
    generateModuleMap.moduleInformation = moduleInformation;
    generateModuleMap.compiledModuleSuffix = product.cpp.compiledModuleSuffix;
    generateModuleMap.moduleInfoSuffix = product.cpp.moduleInfoSuffix;
    generateModuleMap.moduleOutputFlag = product.cpp.moduleOutputFlag;
    generateModuleMap.moduleFileFlag = product.cpp.moduleFileFlag;
    generateModuleMap.toolchain = product.qbs.toolchain;
    generateModuleMap.toolchainType = product.qbs.toolchainType;
    generateModuleMap.modulesFromDeps = modulesFromDeps;
    generateModuleMap.description = "generating module map for " + input.fileName
    generateModuleMap.highlight = "filegen"
    generateModuleMap.sourceCode = function() {
        function moduleFileOption(moduleName) {
            return moduleFileFlag.replace("%module%", moduleName);
        }
        function moduleOutputOption(moduleName) {
            return moduleOutputFlag.replace("%module%", moduleName);
        }

        var content = "";
        if (toolchainType === "gcc" || toolchainType === "mingw")
            content += "$root .\n";
        const isModule = input.fileTags.includes("cppm");
        function modulePath(moduleName) {
            return FileInfo.joinPaths(
                product.buildDirectory,
                "cxx-modules",
                moduleName.replace(':', "-") + compiledModuleSuffix);
        }
        const providesModule = moduleInformation.providesModule
        if (isModule && providesModule !== undefined) {
            if (toolchainType === "msvc") {
                if (moduleInformation.isInterfaceModule)
                    content += "-interface\n";
                else
                    content += "-internalPartition\n";
            }
            content += moduleOutputOption(providesModule)
                + modulePath(providesModule) + "\n";
        }

        function isHeaderImport(mod) {
            return mod.length > 2
                && (mod[0] == '<' && mod[mod.length - 1] == '>'
                    || mod[0] == '"' && mod[mod.length - 1] == '"')
        }

        function processImport(mod) {
            if (!isHeaderImport(mod)) {
                const moduleFile = modulesFromDeps[mod] || modulePath(mod);
                content += moduleFileOption(mod) + moduleFile + "\n";

                function getModuleInfo(moduleFile) {
                    const moduleInfoPath = FileInfo.joinPaths(
                        FileInfo.path(moduleFile), FileInfo.completeBaseName(moduleFile) + moduleInfoSuffix);
                    var file = new TextFile(moduleInfoPath, TextFile.ReadOnly);
                    const moduleInfo = JSON.parse(file.readAll());
                    file.close();
                    return moduleInfo;
                }

                function processImportsRecursive(moduleFile) {
                    // can we do better rather than pass info via a file?
                    const moduleInfo = getModuleInfo(moduleFile);
                    const requiresModules = moduleInfo['requiresModules'] || [];
                    requiresModules.forEach(function(importModule) {
                        const importModuleFile = modulesFromDeps[importModule] || modulePath(importModule);
                        content += moduleFileOption(importModule) + importModuleFile + "\n";
                        processImportsRecursive(importModuleFile);
                    });
                }

                processImportsRecursive(moduleFile);
            }
        }
        moduleInformation.requiresModules.forEach(processImport);

        // converts UTF string to an array of bytes
        function encode(string) {
            var bytes = [];
            for (var i = 0; i < string.length; i++) {
                bytes.push(string.charCodeAt(i));
            }
            return bytes;
        }

        // MinGW fails if file contains \r on Windows, so we use Unix line-endings on all platforms
        // and thus we use BinaryFile instead of TextFile here
        var file = new BinaryFile(outPath, TextFile.WriteOnly);
        file.write(encode(content));
        file.close();
    }

    return generateModuleMap;
}

function prepareModules(project, product, inputs, outputs, input, output) {
    var commands = [];

    // module map is present for both cppm and regular cpp files if they use modules
    const moduleMapOutputs = outputs["modulemap"];
    if (moduleMapOutputs === undefined)
        return commands;

    const cppModuleMap = moduleMapOutputs[0];
    if (cppModuleMap === undefined)
        return commands;

    const moduleInformation = {
        providesModule: cppModuleMap.cpp._providesModule,
        isInterfaceModule: cppModuleMap.cpp._isInterfaceModule,
        requiresModules: cppModuleMap.cpp._requiresModules,
    };

    // module info json is present only for cppm
    var cppModuleInfo = outputs["moduleinfo"];
    if (cppModuleInfo)
        commands.push(prepareModuleInfo(moduleInformation, input, cppModuleInfo[0], product));

    commands.push(prepareModuleMap(moduleInformation, input, cppModuleMap, product));

    return commands;
}