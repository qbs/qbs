/****************************************************************************
**
** Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
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

var Cpp = require("cpp.js");
var Environment = require("qbs.Environment");
var File = require("qbs.File");
var FileInfo = require("qbs.FileInfo");
var ModUtils = require("qbs.ModUtils");
var Process = require("qbs.Process");
var TemporaryDir = require("qbs.TemporaryDir");
var TextFile = require("qbs.TextFile");
var Utilities = require("qbs.Utilities");
var WindowsUtils = require("qbs.WindowsUtils");

function guessArchitecture(macros)
{
    if (macros["__C51__"])
        return "mcs51";
}

function guessEndianness(macros)
{
    // The 8051 processors are 8-bit. So, the data with an integer type
    // represented by more than one byte is stored as big endian in the
    // Keil toolchain. See for more info:
    // * http://www.keil.com/support/man/docs/c51/c51_ap_2bytescalar.htm
    // * http://www.keil.com/support/man/docs/c51/c51_ap_4bytescalar.htm
    return "big";
}

function guessVersion(macros)
{
    var version = macros["__C51__"];
    if (version) {
        versionMajor = parseInt(version / 100);
        versionMinor = parseInt(version % 100);
        versionPatch = 0;
        return { major: versionMajor,
            minor: versionMinor,
            patch: versionPatch,
            found: true }
    }
}

function dumpMacros(compilerFilePath, qbs) {

    // Note: The KEIL compiler (at least for 8051) does not support the predefined
    // macros dumping. So, we do it with following trick where we try to compile
    // a temporary file and to parse the console output.

    function createDumpMacrosFile() {
        var td = new TemporaryDir();
        var fn = FileInfo.fromNativeSeparators(td.path() + "/dump-macros.c");
        var tf = new TextFile(fn, TextFile.WriteOnly);
        tf.writeLine("#define VALUE_TO_STRING(x) #x");
        tf.writeLine("#define VALUE(x) VALUE_TO_STRING(x)");
        tf.writeLine("#define VAR_NAME_VALUE(var) \"\"\"|\"#var\"|\"VALUE(var)");
        tf.writeLine("#ifdef __C51__");
        tf.writeLine("#pragma message(VAR_NAME_VALUE(__C51__))");
        tf.writeLine("#endif");
        tf.close();
        return fn;
    }

    var fn = createDumpMacrosFile();
    var p = new Process();
    p.exec(compilerFilePath, [ fn ], true);
    var map = {};
    p.readStdOut().trim().split(/\r?\n/g).map(function(line) {
        var parts = line.split("\"|\"", 3);
        map[parts[1]] = parts[2];
    });
    return map;
}

function adjustPathsToWindowsSeparators(sourcePaths) {
    var resulingPaths = [];
    sourcePaths.forEach(function(path) {
        resulingPaths.push(FileInfo.toWindowsSeparators(path));
    });
    return resulingPaths;
}

function generateDefineDirective(allDefines) {
    var adjusted = adjustPathsToWindowsSeparators(allDefines);
    return "DEFINE (" + adjusted.join(",") + ")";
}

function generateIncludePathsDirective(allIncludePaths) {
    var adjusted = adjustPathsToWindowsSeparators(allIncludePaths);
    return "INCDIR (" + adjusted.join(";") + ")";
}

function generateObjectsLinkerDirective(allObjectPaths) {
    var adjusted = adjustPathsToWindowsSeparators(allObjectPaths);
    return adjusted.join(",");
}

function generateObjectOutputDirective(outputFilePath) {
    return "OBJECT (" + FileInfo.toWindowsSeparators(outputFilePath) + ")";
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
            return Array.isArray(a) ? a : [];
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

function filterStdOutput(cmd) {
    cmd.stdoutFilterFunction = function(output) {
        // Allow only the error and warning messages
        // with its sub-content.
        var sourceLines = output.split('\n');
        var filteredLines = [];
        for (var i in sourceLines) {
            if (sourceLines[i].startsWith("***")
                || sourceLines[i].startsWith(">>")
                || sourceLines[i].startsWith("    ")) {
                    filteredLines.push(sourceLines[i]);
            }
        }
        return filteredLines.join('\n');
    };
}

function compilerFlags(project, product, input, output, explicitlyDependsOn) {
    // Determine which C-language we"re compiling.
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(output.fileTags));

    var args = [];
    args.push(FileInfo.toWindowsSeparators(input.filePath));
    args.push(generateObjectOutputDirective(output.filePath));

    switch (input.cpp.optimization) {
    case "small":
        args.push("OPTIMIZE (SIZE)");
        break;
    case "fast":
        args.push("OPTIMIZE (SPEED)");
        break;
    case "none":
        args.push("OPTIMIZE (0)");
        break;
    }

    if (input.cpp.debugInformation)
        args.push("DEBUG");

    var warnings = input.cpp.warningLevel;
    if (warnings === "none") {
        args.push("WARNINGLEVEL (0)");
    } else if (warnings === "all") {
        args.push("WARNINGLEVEL (2)");
        args.push("FARWARNING");
    }

    var allDefines = [];
    var platformDefines = input.cpp.platformDefines;
    if (platformDefines)
        allDefines = allDefines.uniqueConcat(platformDefines);
    var defines = input.cpp.defines;
    if (defines)
        allDefines = allDefines.uniqueConcat(defines);
    if (allDefines.length > 0)
        args = args.concat(generateDefineDirective(allDefines));

    var allIncludePaths = [];
    var includePaths = input.cpp.includePaths;
    if (includePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(includePaths);
    var systemIncludePaths = input.cpp.systemIncludePaths;
    if (systemIncludePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(systemIncludePaths);
    var compilerIncludePaths = input.cpp.compilerIncludePaths;
    if (compilerIncludePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(compilerIncludePaths);
    if (allIncludePaths.length > 0)
        args = args.concat(generateIncludePathsDirective(allIncludePaths));

    args = args.concat(ModUtils.moduleProperty(input, "platformFlags"),
                       ModUtils.moduleProperty(input, "flags"),
                       ModUtils.moduleProperty(input, "platformFlags", tag),
                       ModUtils.moduleProperty(input, "flags", tag),
                       ModUtils.moduleProperty(input, "driverFlags", tag));
    return args;
}

function assemblerFlags(project, product, input, output, explicitlyDependsOn) {
    // Determine which C-language we"re compiling
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(output.fileTags));

    var args = [];
    args.push(FileInfo.toWindowsSeparators(input.filePath));
    args.push(generateObjectOutputDirective(output.filePath));

    if (input.cpp.debugInformation)
        args.push("DEBUG");

    var allDefines = [];
    var platformDefines = input.cpp.platformDefines;
    if (platformDefines)
        allDefines = allDefines.uniqueConcat(platformDefines);
    var defines = input.cpp.defines;
    if (defines)
        allDefines = allDefines.uniqueConcat(defines);
    if (allDefines.length > 0)
        args = args.concat(generateDefineDirective(allDefines));

    var allIncludePaths = [];
    var includePaths = input.cpp.includePaths;
    if (includePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(includePaths);
    var systemIncludePaths = input.cpp.systemIncludePaths;
    if (systemIncludePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(systemIncludePaths);
    var compilerIncludePaths = input.cpp.compilerIncludePaths;
    if (compilerIncludePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(compilerIncludePaths);
    if (allIncludePaths.length > 0)
        args = args.concat(generateIncludePathsDirective(allIncludePaths));

    args = args.concat(ModUtils.moduleProperty(input, "platformFlags", tag),
                       ModUtils.moduleProperty(input, "flags", tag),
                       ModUtils.moduleProperty(input, "driverFlags", tag));
    return args;
}

function linkerFlags(project, product, input, outputs) {
    var args = [];
    var allObjectPaths = [];

    function addObjectPath(obj) {
        allObjectPaths.push(obj.filePath);
    }

    // Add all object files.
    if (inputs.obj)
        inputs.obj.map(function(obj) { addObjectPath(obj) });

    // Add all library dependencies.
    var libraryDependencies = collectLibraryDependencies(product);
    if (libraryDependencies) {
        libraryDependencies.forEach(function(staticLibrary) {
            addObjectPath(staticLibrary);
        })
    }

    args = args.concat(generateObjectsLinkerDirective(allObjectPaths));

    // We need to wrap a output file name with quotes. Otherwise
    // the linker will ignore a specified file name.
    args.push("TO", '"' + FileInfo.toWindowsSeparators(outputs.application[0].filePath) + '"');

    if (!product.cpp.generateMapFile)
        args.push("NOMAP");

    args = args.concat(ModUtils.moduleProperty(product, "driverLinkerFlags"));
    return args;
}

function archiverFlags(project, product, input, outputs) {
    var args = [ "TRANSFER" ];
    var allObjectPaths = [];

    function addObjectPath(obj) {
        allObjectPaths.push(obj.filePath);
    }

    if (inputs.obj)
        inputs.obj.map(function(obj) { addObjectPath(obj) });

    // We need to wrap a output file name with quotes. Otherwise
    // the linker will ignore a specified file name.
    args = args.concat(generateObjectsLinkerDirective(allObjectPaths));

    args.push("TO", '"' + FileInfo.toWindowsSeparators(outputs.staticlibrary[0].filePath) + '"');
    return args;
}

function prepareCompiler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var args = compilerFlags(project, product, input, output, explicitlyDependsOn);
    var compilerPath = input.cpp.compilerPath;
    var cmd = new Command(compilerPath, args)
    cmd.description = "compiling " + input.fileName;
    cmd.highlight = "compiler";
    cmd.maxExitCode = 1;
    filterStdOutput(cmd);
    return [cmd];
}


function prepareAssembler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var args = assemblerFlags(project, product, input, output, explicitlyDependsOn);
    var assemblerPath = input.cpp.assemblerPath;
    var cmd = new Command(assemblerPath, args)
    cmd.description = "assembling " + input.fileName;
    cmd.highlight = "compiler";
    filterStdOutput(cmd);
    return [cmd];
}

function prepareLinker(project, product, inputs, outputs, input, output) {
    var primaryOutput = outputs.application[0];
    var args = linkerFlags(project, product, input, outputs);
    var linkerPath = product.cpp.linkerPath;
    var cmd = new Command(linkerPath, args)
    cmd.description = "linking " + primaryOutput.fileName;
    cmd.highlight = "linker";
    filterStdOutput(cmd);
    return [cmd];
}

function prepareArchiver(project, product, inputs, outputs, input, output) {
    var args = archiverFlags(project, product, input, outputs);
    var archiverPath = product.cpp.archiverPath;
    var cmd = new Command(archiverPath, args)
    cmd.description = "linking " + output.fileName;
    cmd.highlight = "linker";
    return [cmd];
}
