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

function supportXLinker(architecture) {
    return architecture === "78k" || architecture === "avr"
        || architecture === "avr32" || architecture === "mcs51"
        || architecture === "msp430" || architecture === "v850";
}

function supportILinker(architecture) {
    return architecture.startsWith("arm")
        || architecture === "rh850" || architecture === "rl78"
        || architecture === "rx" || architecture === "stm8";
}

function compilerName(qbs) {
    var architecture = qbs.architecture;
    if (architecture.startsWith("arm"))
        return  "iccarm";
    else if (architecture === "mcs51")
        return "icc8051";
    else if (architecture === "avr")
        return "iccavr";
    else if (architecture === "stm8")
        return "iccstm8";
    else if (architecture === "msp430")
        return "icc430";
    else if (architecture === "v850")
        return "iccv850";
    else if (architecture === "78k")
        return "icc78k";
    else if (architecture === "rl78")
        return "iccrl78";
    else if (architecture === "rx")
        return "iccrx";
    else if (architecture === "rh850")
        return "iccrh850";
    else if (architecture === "avr32")
        return "iccavr32";
    throw "Unable to deduce compiler name for unsupported architecture: '"
            + architecture + "'";
}

function assemblerName(qbs) {
    var architecture = qbs.architecture;
    if (architecture.startsWith("arm"))
        return "iasmarm";
    else if (architecture === "rl78")
        return "iasmrl78";
    else if (architecture === "rx")
        return "iasmrx";
    else if (architecture === "rh850")
        return "iasmrh850";
    else if (architecture === "mcs51")
        return "a8051";
    else if (architecture === "avr")
        return "aavr";
    else if (architecture === "stm8")
        return "iasmstm8";
    else if (architecture === "msp430")
        return "a430";
    else if (architecture === "v850")
        return "av850";
    else if (architecture === "78k")
        return "a78k";
    else if (architecture === "avr32")
        return "aavr32";
    throw "Unable to deduce assembler name for unsupported architecture: '"
            + architecture + "'";
}

function linkerName(qbs) {
    var architecture = qbs.architecture;
    if (supportXLinker(architecture))
        return "xlink";
    else if (supportILinker(architecture))
        return "ilink" + architecture;
    throw "Unable to deduce linker name for unsupported architecture: '"
            + architecture + "'";
}

function archiverName(qbs) {
    var architecture = qbs.architecture;
    if (architecture.startsWith("arm")
            || architecture === "stm8" || architecture === "rl78"
            || architecture === "rx" || architecture === "rh850") {
        return "iarchive";
    } else if (architecture === "mcs51" || architecture === "avr"
               || architecture === "msp430" || architecture === "v850"
               || architecture === "78k" || architecture === "avr32") {
        return "xlib";
    }
    throw "Unable to deduce archiver name for unsupported architecture: '"
            + architecture + "'";
}

function staticLibrarySuffix(qbs) {
    var architecture = qbs.architecture;
    if (architecture.startsWith("arm")
            || architecture === "stm8" || architecture === "rl78"
            || architecture === "rx" || architecture === "rh850") {
        return ".a";
    } else if (architecture === "mcs51") {
        return ".r51";
    } else if (architecture === "avr") {
        return ".r90";
    } else if (architecture === "msp430") {
        return ".r43";
    } else if (architecture === "v850") {
        return ".r85";
    } else if (architecture === "78k") {
        return ".r26";
    } else if (architecture === "avr32") {
        return ".r82";
    }
    throw "Unable to deduce static library suffix for unsupported architecture: '"
            + architecture + "'";
}

function executableSuffix(qbs) {
    var architecture = qbs.architecture;
    var isDebug = qbs.debugInformation;
    if (architecture.startsWith("arm")
            || architecture === "stm8" || architecture === "rl78"
            || architecture === "rx" || architecture === "rh850") {
        return ".out";
    } else if (architecture === "mcs51") {
        return isDebug ? ".d51" : ".a51";
    } else if (architecture === "avr") {
        return isDebug ? ".d90" : ".a90";
    } else if (architecture === "msp430") {
        return isDebug ? ".d43" : ".a43";
    } else if (architecture === "v850") {
        return isDebug ? ".d85" : ".a85";
    } else if (architecture === "78k") {
        return isDebug ? ".d26" : ".a26";
    } else if (architecture === "avr32") {
        return isDebug ? ".d82" : ".a82";
    }
    throw "Unable to deduce executable suffix for unsupported architecture: '"
            + architecture + "'";
}

function objectSuffix(qbs) {
    var architecture = qbs.architecture;
    if (architecture.startsWith("arm")
            || architecture === "stm8" || architecture === "rl78"
            || architecture === "rx" || architecture === "rh850") {
        return ".o";
    } else if (architecture === "mcs51") {
        return ".r51";
    } else if (architecture === "avr") {
        return ".r90";
    } else if (architecture === "msp430") {
        return ".r43";
    } else if (architecture === "v850") {
        return ".r85";
    } else if (architecture === "78k") {
        return ".r26";
    } else if (architecture === "avr32") {
        return ".r82";
    }
    throw "Unable to deduce object file suffix for unsupported architecture: '"
            + architecture + "'";
}

function imageFormat(qbs) {
    var architecture = qbs.architecture;
    if (architecture.startsWith("arm")
            || architecture === "stm8" || architecture === "rl78"
            || architecture === "rx" || architecture === "rh850") {
        return "elf";
    } else if (architecture=== "mcs51" || architecture === "avr"
               || architecture === "msp430" || architecture === "v850"
               || architecture === "78k" || architecture === "avr32") {
        return "ubrof";
    }
    throw "Unable to deduce image format for unsupported architecture: '"
            + architecture + "'";
}

function guessArmArchitecture(core) {
    var arch = "arm";
    if (core === "__ARM4M__")
        arch += "v4m";
    else if (core === "__ARM4TM__")
        arch += "v4tm";
    else if (core === "__ARM5__")
        arch += "v5";
    else if (core === "__ARM5E__")
        arch += "v5e";
    else if (core === "__ARM6__")
        arch += "v6";
    else if (core === "__ARM6M__")
        arch += "v6m";
    else if (core === "__ARM6SM__")
        arch += "v6sm";
    else if (core === "__ARM7M__")
        arch += "v7m";
    else if (core === "__ARM7R__")
        arch += "v7r";
    return arch;
}

function guessArchitecture(macros) {
    if (macros["__ICCARM__"] === "1")
        return guessArmArchitecture(macros["__CORE__"]);
    else if (macros["__ICC8051__"] === "1")
        return "mcs51";
    else if (macros["__ICCAVR__"] === "1")
        return "avr";
    else if (macros["__ICCSTM8__"] === "1")
        return "stm8";
    else if (macros["__ICC430__"] === "1")
        return "msp430";
    else if (macros["__ICCRL78__"] === "1")
        return "rl78";
    else if (macros["__ICCRX__"] === "1")
        return "rx";
    else if (macros["__ICCRH850__"] === "1")
        return "rh850";
    else if (macros["__ICCV850__"] === "1")
        return "v850";
    else if (macros["__ICC78K__"] === "1")
        return "78k";
    else if (macros["__ICCAVR32__"] === "1")
        return "avr32";
}

function guessEndianness(macros) {
    if (macros["__LITTLE_ENDIAN__"] === "1")
        return "little";
    return "big"
}

function guessVersion(macros, architecture)
{
    var version = parseInt(macros["__VER__"], 10);
    if (architecture.startsWith("arm")) {
        return { major: parseInt(version / 1000000),
            minor: parseInt(version / 1000) % 1000,
            patch: parseInt(version) % 1000,
            found: true }
    } else if (architecture === "mcs51" || architecture === "avr" || architecture === "stm8"
               || architecture === "msp430" || architecture === "rl78" || architecture === "rx"
               || architecture === "rh850" || architecture === "v850" || architecture === "78k"
               || architecture === "avr32") {
        return { major: parseInt(version / 100),
            minor: parseInt(version % 100),
            patch: 0,
            found: true }
    }
}

function cppLanguageOption(compilerFilePath) {
    var baseName = FileInfo.baseName(compilerFilePath);
    switch (baseName) {
    case "iccarm":
    case "iccrl78":
    case "iccrx":
    case "iccrh850":
        return "--c++";
    case "icc8051":
    case "iccavr":
    case "iccstm8":
    case "icc430":
    case "iccv850":
    case "icc78k":
    case "iccavr32":
        return "--ec++";
    }
    throw "Unable to deduce C++ language option for unsupported compiler: '"
            + FileInfo.toNativeSeparators(compilerFilePath) + "'";
}

function dumpMacros(compilerFilePath, tag) {
    var tempDir = new TemporaryDir();
    var inFilePath = FileInfo.fromNativeSeparators(tempDir.path() + "/empty-source.c");
    var inFile = new TextFile(inFilePath, TextFile.WriteOnly);
    var outFilePath = FileInfo.fromNativeSeparators(tempDir.path() + "/iar-macros.predef");

    var args = [ inFilePath, "--predef_macros", outFilePath ];
    if (tag && tag === "cpp")
        args.push(cppLanguageOption(compilerFilePath));

    var p = new Process();
    p.exec(compilerFilePath, args, true);
    var outFile = new TextFile(outFilePath, TextFile.ReadOnly);
    var map = {};
    outFile.readAll().trim().split(/\r?\n/g).map(function (line) {
            var parts = line.split(" ", 3);
            map[parts[1]] = parts[2];
        });
    return map;
}

function dumpDefaultPaths(compilerFilePath, tag) {
    var tempDir = new TemporaryDir();
    var inFilePath = FileInfo.fromNativeSeparators(tempDir.path() + "/empty-source.c");
    var inFile = new TextFile(inFilePath, TextFile.WriteOnly);

    var args = [ inFilePath, "--preinclude", "." ];
    if (tag === "cpp")
        args.push(cppLanguageOption(compilerFilePath));

    var p = new Process();
    // This process should return an error, don't throw
    // an error in this case.
    p.exec(compilerFilePath, args, false);
    var output = p.readStdErr();

    var includePaths = [];
    var pass = 0;
    for (var pos = 0; pos < output.length; ++pos) {
        var searchIndex = output.indexOf("searched:", pos);
        if (searchIndex === -1)
            break;
        var startQuoteIndex = output.indexOf('"', searchIndex + 1);
        if (startQuoteIndex === -1)
            break;
        var endQuoteIndex = output.indexOf('"', startQuoteIndex + 1);
        if (endQuoteIndex === -1)
            break;
        pos = endQuoteIndex + 1;

        // Ignore the first path as it is not a compiler include path.
        ++pass;
        if (pass === 1)
            continue;

        var parts = output.substring(startQuoteIndex + 1, endQuoteIndex).split("\n");
        var includePath = "";
        for (var i in parts)
            includePath += parts[i].trim();

        includePaths.push(includePath);
    }

    return {
        "includePaths": includePaths
    };
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

function compilerOutputArtifacts(input, useListing) {
    var artifacts = [];
    artifacts.push({
        fileTags: ["obj"],
        filePath: Utilities.getHash(input.baseDir) + "/"
              + input.fileName + input.cpp.objectSuffix
    });
    if (useListing) {
        artifacts.push({
            fileTags: ["lst"],
            filePath: Utilities.getHash(input.baseDir) + "/"
              + input.fileName + ".lst"
        });
    }
    return artifacts;
}

function applicationLinkerOutputArtifacts(product) {
    var app = {
        fileTags: ["application"],
        filePath: FileInfo.joinPaths(
                      product.destinationDirectory,
                      PathTools.applicationFilePath(product))
    };
    var mem_map = {
        fileTags: ["mem_map"],
        filePath: FileInfo.joinPaths(
                      product.destinationDirectory,
                      product.targetName + ".map")
    };
    return [app, mem_map]
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

function compilerFlags(project, product, input, outputs, explicitlyDependsOn) {
    // Determine which C-language we're compiling.
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(outputs.obj[0].fileTags));

    var args = [];

    // Input.
    args.push(input.filePath);

    // Output.
    args.push("-o", outputs.obj[0].filePath);

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

    // Silent output generation flag.
    args.push("--silent");

    // Debug information flags.
    if (input.cpp.debugInformation)
        args.push("--debug");

    // Optimization flags.
    switch (input.cpp.optimization) {
    case "small":
        args.push("-Ohs");
        break;
    case "fast":
        args.push("-Ohz");
        break;
    case "none":
        args.push("-On");
        break;
    }

    var architecture = input.qbs.architecture;

    // Warning level flags.
    switch (input.cpp.warningLevel) {
    case "none":
        args.push("--no_warnings");
        break;
    case "all":
        if (architecture !== "78k") {
            args.push("--deprecated_feature_warnings="
                +"+attribute_syntax,"
                +"+preprocessor_extensions,"
                +"+segment_pragmas");
            if (tag === "cpp")
                args.push("--warn_about_c_style_casts");
        }
        break;
    }
    if (input.cpp.treatWarningsAsErrors)
        args.push("--warnings_are_errors");

    // C language version flags.
    if (tag === "c" && (architecture !== "78k")) {
        var knownValues = ["c89"];
        var cLanguageVersion = Cpp.languageVersion(
                    input.cpp.cLanguageVersion, knownValues, "C");
        switch (cLanguageVersion) {
        case "c89":
            args.push("--c89");
            break;
        default:
            // Default C language version is C11/C99 that
            // depends on the IAR version.
            break;
        }
    }

    // C++ language version flags.
    if (tag === "cpp") {
        if (architecture.startsWith("arm")
                || architecture === "rl78" || architecture === "rx"
                || architecture ===  "rh850") {
            // Enable C++ language flags.
            args.push("--c++");
            // Exceptions flags.
            if (!input.cpp.enableExceptions)
                args.push("--no_exceptions");
            // RTTI flags.
            if (!input.cpp.enableRtti)
                args.push("--no_rtti");
        } else if (architecture === "stm8" || architecture === "mcs51"
                   || architecture === "avr" || architecture === "msp430"
                   || architecture === "v850" || architecture === "78k"
                   || architecture === "avr32") {
            args.push("--ec++");
        }
    }

    // Byte order flags.
    if (architecture.startsWith("arm") || architecture === "rx") {
        var endianness = input.cpp.endianness;
        if (endianness)
            args.push("--endian=" + endianness);
    }

    // Listing files generation flag.
    if (product.cpp.generateCompilerListingFiles)
        args.push("-l", outputs.lst[0].filePath);

    // Misc flags.
    args = args.concat(ModUtils.moduleProperty(input, "platformFlags"),
                       ModUtils.moduleProperty(input, "flags"),
                       ModUtils.moduleProperty(input, "platformFlags", tag),
                       ModUtils.moduleProperty(input, "flags", tag),
                       ModUtils.moduleProperty(input, "driverFlags", tag));
    return args;
}

function assemblerFlags(project, product, input, outputs, explicitlyDependsOn) {
    // Determine which C-language we"re compiling
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(outputs.obj[0].fileTags));

    var args = [];

    // Input.
    args.push(input.filePath);

    // Output.
    args.push("-o", outputs.obj[0].filePath);

    // Includes.
    var allIncludePaths = [];
    var systemIncludePaths = input.cpp.systemIncludePaths;
    if (systemIncludePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(systemIncludePaths);
    var compilerIncludePaths = input.cpp.compilerIncludePaths;
    if (compilerIncludePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(compilerIncludePaths);
    args = args.concat(allIncludePaths.map(function(include) { return "-I" + include }));

    // Debug information flags.
    if (input.cpp.debugInformation)
        args.push("-r");

    // Architecture specific flags.
    var architecture = input.qbs.architecture;
    if (architecture === "stm8" || architecture === "rl78"
            || architecture === "rx" || architecture === "rh850"
            || architecture === "avr32") {
        // Silent output generation flag.
        args.push("--silent");
        // Warning level flags.
        if (input.cpp.warningLevel === "none")
            args.push("--no_warnings");
        if (input.cpp.treatWarningsAsErrors)
            args.push("--warnings_are_errors");
    } else {
        // Silent output generation flag.
        args.push("-S");
        // Warning level flags.
        args.push("-w" + (input.cpp.warningLevel === "none" ? "-" : "+"));
    }

    // Listing files generation flag.
    if (product.cpp.generateAssemblerListingFiles)
        args.push("-l", outputs.lst[0].filePath);

    // Misc flags.
    args = args.concat(ModUtils.moduleProperty(input, "platformFlags", tag),
                       ModUtils.moduleProperty(input, "flags", tag),
                       ModUtils.moduleProperty(input, "driverFlags", tag));
    return args;
}

function linkerFlags(project, product, input, outputs) {
    var args = [];

    // Inputs.
    if (inputs.obj)
        args = args.concat(inputs.obj.map(function(obj) { return obj.filePath }));

    // Output.
    args.push("-o", outputs.application[0].filePath);

    // Library paths.
    var allLibraryPaths = [];
    var libraryPaths = product.cpp.libraryPaths;
    if (libraryPaths)
        allLibraryPaths = allLibraryPaths.uniqueConcat(libraryPaths);
    var distributionLibraryPaths = product.cpp.distributionLibraryPaths;
    if (distributionLibraryPaths)
        allLibraryPaths = allLibraryPaths.uniqueConcat(distributionLibraryPaths);

    // Library dependencies.
    var libraryDependencies = collectLibraryDependencies(product);
    if (libraryDependencies)
        args = args.concat(libraryDependencies.map(function(dep) { return dep.filePath }));

    // Linker scripts.
    var linkerScripts = inputs.linkerscript
        ? inputs.linkerscript.map(function(a) { return a.filePath; }) : [];

    // Architecture specific flags.
    var architecture = product.qbs.architecture;
    if (supportXLinker(architecture)) {
        args = args.concat(allLibraryPaths.map(function(path) { return '-L' + path }));
        // Silent output generation flag.
        args.push("--silent");
        // Map file generation flag.
        if (product.cpp.generateLinkerMapFile)
            args.push("--map", outputs.mem_map[0].filePath);
        // Entry point flag.
        if (product.cpp.entryPoint)
            args.push("--entry", product.cpp.entryPoint);
        // Linker scripts flags.
        linkerScripts.forEach(function(script) { args.push("--config", script); });
    } else if (supportILinker(architecture)) {
        args = args.concat(allLibraryPaths.map(function(path) { return '-I' + path }));
        // Silent output generation flag.
        args.push("-S");
        // Debug information flag.
        if (product.cpp.debugInformation)
            args.push("-rt");
        // Map file generation flag.
        if (product.cpp.generateLinkerMapFile)
            args.push("-l", outputs.mem_map[0].filePath);
        // Entry point flag.
        if (product.cpp.entryPoint)
            args.push("-s", product.cpp.entryPoint);
        // Linker scripts flags.
        linkerScripts.forEach(function(script) { args.push("-f", script); });
    }

    // Misc flags.
    args = args.concat(ModUtils.moduleProperty(product, "driverLinkerFlags"));
    return args;
}

function archiverFlags(project, product, input, outputs) {
    var args = [];

    // Inputs.
    if (inputs.obj)
        args = args.concat(inputs.obj.map(function(obj) { return obj.filePath }));

    // Output.
    args.push("--create");
    args.push("-o", outputs.staticlibrary[0].filePath);

    return args;
}

function prepareCompiler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var args = compilerFlags(project, product, input, outputs, explicitlyDependsOn);
    var compilerPath = input.cpp.compilerPath;
    var cmd = new Command(compilerPath, args);
    cmd.description = "compiling " + input.fileName;
    cmd.highlight = "compiler";
    return [cmd];
}

function prepareAssembler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var args = assemblerFlags(project, product, input, outputs, explicitlyDependsOn);
    var assemblerPath = input.cpp.assemblerPath;
    var cmd = new Command(assemblerPath, args);
    cmd.description = "assembling " + input.fileName;
    cmd.highlight = "compiler";
    return [cmd];
}

function prepareLinker(project, product, inputs, outputs, input, output) {
    var primaryOutput = outputs.application[0];
    var args = linkerFlags(project, product, input, outputs);
    var linkerPath = product.cpp.linkerPath;
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
    cmd.stdoutFilterFunction = function(output) {
        return "";
    };
    return [cmd];
}
