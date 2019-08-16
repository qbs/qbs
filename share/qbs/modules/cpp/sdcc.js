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

function compilerName(qbs) {
    return "sdcc";
}

function assemblerName(qbs) {
    switch (qbs.architecture) {
    case "mcs51":
        return "sdas8051";
    case "stm8":
        return "sdasstm8";
    }
    throw "Unable to deduce assembler name for unsupported architecture: '"
            + qbs.architecture + "'";
}

function linkerName(qbs) {
    switch (qbs.architecture) {
    case "mcs51":
        return "sdld";
    case "stm8":
        return "sdldstm8";
    }
    throw "Unable to deduce linker name for unsupported architecture: '"
            + qbs.architecture + "'";
}

function archiverName(qbs) {
    return "sdcclib";
}

function targetArchitectureFlag(architecture) {
    if (architecture === "mcs51")
        return "-mmcs51";
    if (architecture === "stm8")
        return "-mstm8";
}

function guessArchitecture(macros) {
    if (macros["__SDCC_mcs51"] === "1")
        return "mcs51";
    if (macros["__SDCC_stm8"] === "1")
        return "stm8";
}

function guessEndianness(macros) {
    // SDCC stores numbers in little-endian format.
    return "little";
}

function guessVersion(macros) {
    return { major: parseInt(macros["__SDCC_VERSION_MAJOR"], 10),
        minor: parseInt(macros["__SDCC_VERSION_MINOR"], 10),
        patch: parseInt(macros["__SDCC_VERSION_PATCH"], 10),
        found: macros["SDCC"] }
}

function dumpMacros(compilerFilePath, architecture) {
    var tempDir = new TemporaryDir();
    var fakeIn = new TextFile(tempDir.path() + "/empty-source.c", TextFile.WriteOnly);
    var args = [ fakeIn.filePath(), "-dM", "-E" ];

    var targetFlag = targetArchitectureFlag(architecture);
    if (targetFlag)
        args.push(targetFlag);

    var p = new Process();
    p.exec(compilerFilePath, args, true);
    var map = {};
    p.readStdOut().trim().split(/\r?\n/g).map(function (line) {
        var parts = line.split(" ", 3);
        map[parts[1]] = parts[2];
    });
    return map;
}

function dumpDefaultPaths(compilerFilePath, architecture) {
    var args = [ "--print-search-dirs" ];

    var targetFlag = targetArchitectureFlag(architecture);
    if (targetFlag)
        args.push(targetFlag);

    var p = new Process();
    p.exec(compilerFilePath, args, true);
    var includePaths = [];
    var addIncludePaths = false;
    var lines = p.readStdOut().trim().split(/\r?\n/g);
    for (var i in lines) {
        var line = lines[i];
        if (line.startsWith("includedir:")) {
            addIncludePaths = true;
        } else if (line.startsWith("programs:")
                    || line.startsWith("datadir:")
                    || line.startsWith("libdir:")
                    || line.startsWith("libpath:")) {
            addIncludePaths = false;
        } else if (addIncludePaths) {
            includePaths.push(line);
        }
    }

    return {
        "includePaths": includePaths
    }
}

function effectiveLinkerPath(product) {
    if (product.cpp.linkerMode === "automatic")
        return product.cpp.compilerPath;
    return product.cpp.linkerPath;
}

function useCompilerDriverLinker(product) {
    var linker = effectiveLinkerPath(product);
    return linker === product.cpp.compilerPath;
}

function escapeLinkerFlags(product, linkerFlags) {
    if (!linkerFlags || linkerFlags.length === 0 || !useCompilerDriverLinker(product))
        return linkerFlags;
    return ["-Wl " + linkerFlags.join(",")];
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

function compilerFlags(project, product, input, output, explicitlyDependsOn) {
    // Determine which C-language we"re compiling.
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(output.fileTags));

    var args = [];

    // Input.
    args.push(input.filePath);

    // Output.
    args.push("-c");
    args.push("-o", output.filePath);

    // Defines.
    var allDefines = [];
    var platformDefines = input.cpp.platformDefines;
    if (platformDefines)
        allDefines = allDefines.uniqueConcat(platformDefines);
    var defines = input.cpp.defines;
    if (defines)
        allDefines = allDefines.uniqueConcat(defines);
    args = args.concat(allDefines.map(function(define) { return "-D" + define }));

    // Includes.
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
    args = args.concat(allIncludePaths.map(function(include) { return "-I" + include }));

    var targetFlag = targetArchitectureFlag(input.cpp.architecture);
    if (targetFlag)
        args.push(targetFlag);

    // Debug information flags.
    if (input.cpp.debugInformation)
        args.push("--debug");

    // Optimization level flags.
    switch (input.cpp.optimization) {
    case "small":
        args.push("--opt-code-size");
        break;
    case "fast":
        args.push("--opt-code-speed");
        break;
    case "none":
        // SDCC has not option to disable the optimization.
        break;
    }

    // Warning level flags.
    if (input.cpp.warningLevel === "none")
        args.push("--less-pedantic");
    if (input.cpp.treatWarningsAsErrors)
        args.push("--Werror");

    // C language version flags.
    if (tag === "c") {
        var knownValues = ["c11", "c89"];
        var cLanguageVersion = Cpp.languageVersion(
                    input.cpp.cLanguageVersion, knownValues, "C");
        switch (cLanguageVersion) {
        case "c89":
            args.push("--std-c89");
            break;
        case "c11":
            args.push("--std-c11");
            break;
        }
    }

    // Misc flags.
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

    // Input.
    args.push(input.filePath);

    // Output.
    args.push("-o", output.filePath);

    // Includes.
    var allIncludePaths = [];
    var systemIncludePaths = input.cpp.systemIncludePaths;
    if (systemIncludePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(systemIncludePaths);
    var compilerIncludePaths = input.cpp.compilerIncludePaths;
    if (compilerIncludePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(compilerIncludePaths);
    args = args.concat(allIncludePaths.map(function(include) { return "-I" + include }));

    // Misc flags.
    args = args.concat(ModUtils.moduleProperty(input, "platformFlags", tag),
                       ModUtils.moduleProperty(input, "flags", tag),
                       ModUtils.moduleProperty(input, "driverFlags", tag));
    return args;
}

function linkerFlags(project, product, input, outputs) {
    var args = [];

    // Target MCU flag.
    var targetFlag = targetArchitectureFlag(product.cpp.architecture);
    if (targetFlag)
        args.push(targetFlag);

    var allLibraryPaths = [];
    var libraryPaths = product.cpp.libraryPaths;
    if (libraryPaths)
        allLibraryPaths = allLibraryPaths.uniqueConcat(libraryPaths);
    var distributionLibraryPaths = product.cpp.distributionLibraryPaths;
    if (distributionLibraryPaths)
        allLibraryPaths = allLibraryPaths.uniqueConcat(distributionLibraryPaths);

    var libraryDependencies = collectLibraryDependencies(product);

    var escapableLinkerFlags = [];

    // Map file generation flag.
    if (product.cpp.generateMapFile)
        escapableLinkerFlags.push("-m");

    if (product.cpp.platformLinkerFlags)
        Array.prototype.push.apply(escapableLinkerFlags, product.cpp.platformLinkerFlags);
    if (product.cpp.linkerFlags)
        Array.prototype.push.apply(escapableLinkerFlags, product.cpp.linkerFlags);

    var useCompilerDriver = useCompilerDriverLinker(product);
    if (useCompilerDriver) {
        // Output.
        args.push("-o", outputs.application[0].filePath);

        // Inputs.
        if (inputs.obj)
            args = args.concat(inputs.obj.map(function(obj) { return obj.filePath }));

        // Library paths.
        args = args.concat(allLibraryPaths.map(function(path) { return "-L" + path }));

        // Linker scripts.
        var scripts = inputs.linkerscript
            ? inputs.linkerscript.map(function(scr) { return "-f" + scr.filePath; }) : [];
        if (scripts)
            Array.prototype.push.apply(escapableLinkerFlags, scripts);
    } else {
        // Output.
        args.push(outputs.application[0].filePath);

        // Inputs.
        if (inputs.obj)
            args = args.concat(inputs.obj.map(function(obj) { return obj.filePath }));

        // Library paths.
        args = args.concat(allLibraryPaths.map(function(path) { return "-k" + path }));

        // Linker scripts.
        // Note: We need to split the '-f' and the file path to separate
        // lines; otherwise the linking fails.
        inputs.linkerscript.forEach(function(scr) {
            escapableLinkerFlags.push("-f", scr.filePath);
        });
    }

    // Library dependencies.
    if (libraryDependencies)
        args = args.concat(libraryDependencies.map(function(dep) { return "-l" + dep.filePath }));

    // Misc flags.
    var escapedLinkerFlags = escapeLinkerFlags(product, escapableLinkerFlags);
    if (escapedLinkerFlags)
        Array.prototype.push.apply(args, escapedLinkerFlags);

    return args;
}

function archiverFlags(project, product, input, outputs) {
    var args = [];
    args.push(outputs.staticlibrary[0].filePath);
    if (inputs.obj)
        args = args.concat(inputs.obj.map(function(obj) { return obj.filePath }));
    return args;
}

function prepareCompiler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var args = compilerFlags(project, product, input, output, explicitlyDependsOn);
    var compilerPath = input.cpp.compilerPath;
    var cmd = new Command(compilerPath, args);
    cmd.description = "compiling " + input.fileName;
    cmd.highlight = "compiler";
    return [cmd];
}

function prepareAssembler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var args = assemblerFlags(project, product, input, output, explicitlyDependsOn);
    var assemblerPath = input.cpp.assemblerPath;
    var cmd = new Command(assemblerPath, args);
    cmd.description = "assembling " + input.fileName;
    cmd.highlight = "compiler";
    return [cmd];
}

function prepareLinker(project, product, inputs, outputs, input, output) {
    var primaryOutput = outputs.application[0];
    var args = linkerFlags(project, product, input, outputs);
    var linkerPath = effectiveLinkerPath(product);
    var cmd = new Command(linkerPath, args);
    cmd.description = "linking " + primaryOutput.fileName;
    cmd.highlight = "linker";
    return [cmd];
}

function prepareArchiver(project, product, inputs, outputs, input, output) {
    var args = archiverFlags(project, product, input, outputs);
    var archiverPath = product.cpp.archiverPath;
    var cmd = new Command(archiverPath, args);
    cmd.description = "linking " + output.fileName;
    cmd.highlight = "linker";
    return [cmd];
}
