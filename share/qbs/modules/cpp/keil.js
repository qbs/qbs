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
var PathTools = require("qbs.PathTools");
var Process = require("qbs.Process");
var TemporaryDir = require("qbs.TemporaryDir");
var TextFile = require("qbs.TextFile");
var Utilities = require("qbs.Utilities");

function isMcsArchitecture(architecture) {
    return architecture === "mcs51" || architecture === "mcs251";
}

function isArmArchitecture(architecture) {
    return architecture.startsWith("arm");
}

function compilerName(qbs) {
    var architecture = qbs.architecture;
    if (architecture === "mcs51")
        return "c51";
    if (architecture === "mcs251")
        return "c251";
    if (isArmArchitecture(architecture))
        return "armcc";
    throw "Unable to deduce compiler name for unsupported architecture: '"
            + architecture + "'";
}

function assemblerName(qbs) {
    var architecture = qbs.architecture;
    if (architecture === "mcs51")
        return "a51";
    if (architecture === "mcs251")
        return "a251";
    if (isArmArchitecture(architecture))
        return "armasm";
    throw "Unable to deduce assembler name for unsupported architecture: '"
            + architecture + "'";
}

function linkerName(qbs) {
    var architecture = qbs.architecture;
    if (architecture === "mcs51")
        return "bl51";
    if (architecture === "mcs251")
        return "l251";
    if (isArmArchitecture(architecture))
        return "armlink";
    throw "Unable to deduce linker name for unsupported architecture: '"
            + architecture + "'";
}

function archiverName(qbs) {
    var architecture = qbs.architecture;
    if (architecture === "mcs51")
        return "lib51";
    if (architecture === "mcs251")
        return "lib251";
    if (isArmArchitecture(architecture))
        return "armar";
    throw "Unable to deduce archiver name for unsupported architecture: '"
            + architecture + "'";
}

function staticLibrarySuffix(qbs) {
    var architecture = qbs.architecture;
    if (isMcsArchitecture(architecture) || isArmArchitecture(architecture))
        return ".lib";
    throw "Unable to deduce static library suffix for unsupported architecture: '"
            + architecture + "'";
}

function executableSuffix(qbs) {
    var architecture = qbs.architecture;
    if (isMcsArchitecture(architecture))
        return ".abs";
    if (isArmArchitecture(architecture))
        return ".axf";
    throw "Unable to deduce executable suffix for unsupported architecture: '"
            + architecture + "'";
}

function objectSuffix(qbs) {
    var architecture = qbs.architecture;
    if (isMcsArchitecture(architecture))
        return ".obj";
    if (isArmArchitecture(architecture))
        return ".o";
    throw "Unable to deduce object file suffix for unsupported architecture: '"
            + architecture + "'";
}

function imageFormat(qbs) {
    var architecture = qbs.architecture;
    if (isMcsArchitecture(architecture))
        // Keil OMF51 or OMF2 Object Module Format (which is an
        // extension of the original Intel OMF51).
        return "omf";
    if (isArmArchitecture(architecture))
        return "elf";
    throw "Unable to deduce image format for unsupported architecture: '"
            + architecture + "'";
}

function guessArmArchitecture(targetArchArm, targetArchThumb) {
    var arch = "arm";
    if (targetArchArm === "4" && targetArchThumb === "0")
        arch += "v4";
    else if (targetArchArm === "4" && targetArchThumb === "1")
        arch += "v4t";
    else if (targetArchArm === "5" && targetArchThumb === "2")
        arch += "v5t";
    else if (targetArchArm === "6" && targetArchThumb === "3")
        arch += "v6";
    else if (targetArchArm === "6" && targetArchThumb === "4")
        arch += "v6t2";
    else if (targetArchArm === "0" && targetArchThumb === "3")
        arch += "v6m";
    else if (targetArchArm === "7" && targetArchThumb === "4")
        arch += "v7r";
    else if (targetArchArm === "0" && targetArchThumb === "4")
        arch += "v7m";
    return arch;
}

function guessArchitecture(macros) {
    if (macros["__C51__"])
        return "mcs51";
    else if (macros["__C251__"])
        return "mcs251";
    else if (macros["__CC_ARM"] === "1")
        return guessArmArchitecture(macros["__TARGET_ARCH_ARM"], macros["__TARGET_ARCH_THUMB"]);
}

function guessEndianness(macros) {
    if (macros["__C51__"] || macros["__C251__"]) {
        // The 8051 processors are 8-bit. So, the data with an integer type
        // represented by more than one byte is stored as big endian in the
        // Keil toolchain. See for more info:
        // * http://www.keil.com/support/man/docs/c51/c51_ap_2bytescalar.htm
        // * http://www.keil.com/support/man/docs/c51/c51_ap_4bytescalar.htm
        // * http://www.keil.com/support/man/docs/c251/c251_ap_2bytescalar.htm
        // * http://www.keil.com/support/man/docs/c251/c251_ap_4bytescalar.htm
        return "big";
    } else if (macros["__ARMCC_VERSION"]) {
        return macros["__BIG_ENDIAN"] ? "big" : "little";
    }
}

function guessVersion(macros) {
    if (macros["__C51__"] || macros["__C251__"]) {
        var mcsVersion = macros["__C51__"] || macros["__C251__"];
        return { major: parseInt(mcsVersion / 100),
            minor: parseInt(mcsVersion % 100),
            patch: 0,
            found: true }
    } else if (macros["__CC_ARM"]) {
        var armVersion = macros["__ARMCC_VERSION"];
        return { major: parseInt(armVersion / 1000000),
            minor: parseInt(armVersion / 10000) % 100,
            patch: parseInt(armVersion) % 10000,
            found: true }
    }
}

function dumpMcsCompilerMacros(compilerFilePath, tag) {
    // C51 or C251 compiler support only C language.
    if (tag === "cpp")
        return {};

    // Note: The C51 or C251 compiler does not support the predefined
    // macros dumping. So, we do it with the following trick, where we try
    // to create and compile a special temporary file and to parse the console
    // output with the own magic pattern: (""|"key"|"value"|"").

    function createDumpMacrosFile() {
        var td = new TemporaryDir();
        var fn = FileInfo.fromNativeSeparators(td.path() + "/dump-macros.c");
        var tf = new TextFile(fn, TextFile.WriteOnly);
        tf.writeLine("#define VALUE_TO_STRING(x) #x");
        tf.writeLine("#define VALUE(x) VALUE_TO_STRING(x)");

        // Prepare for C51 compiler.
        tf.writeLine("#if defined(__C51__) || defined(__CX51__)\n");
        tf.writeLine("#  define VAR_NAME_VALUE(var) \"(\"\"\"\"|\"#var\"|\"VALUE(var)\"|\"\"\"\")\"\n");
        tf.writeLine("#  if defined (__C51__)\n");
        tf.writeLine("#    pragma message (VAR_NAME_VALUE(__C51__))\n");
        tf.writeLine("#  endif\n");
        tf.writeLine("#  if defined(__CX51__)\n");
        tf.writeLine("#    pragma message (VAR_NAME_VALUE(__CX51__))\n");
        tf.writeLine("#  endif\n");
        tf.writeLine("#endif\n");

        // Prepare for C251 compiler.
        tf.writeLine("#if defined(__C251__)\n");
        tf.writeLine("#  define VAR_NAME_VALUE(var) \"\"|#var|VALUE(var)|\"\"\n");
        tf.writeLine("#  warning (VAR_NAME_VALUE(__C251__))\n");
        tf.writeLine("#endif\n");
        tf.close();
        return fn;
    }

    var fn = createDumpMacrosFile();
    var p = new Process();
    p.exec(compilerFilePath, [ fn ], false);
    var map = {};
    p.readStdOut().trim().split(/\r?\n/g).map(function(line) {
        var parts = line.split("\"|\"", 4);
        if (parts.length === 4)
            map[parts[1]] = parts[2];
    });
    return map;
}

function dumpArmCompilerMacros(compilerFilePath, tag, nullDevice) {
    var args = [ "-E", "--list-macros", nullDevice ];
    if (tag === "cpp")
        args.push("--cpp");

    var p = new Process();
    p.exec(compilerFilePath, args, false);
    var map = {};
    p.readStdOut().trim().split(/\r?\n/g).map(function (line) {
        if (!line.startsWith("#define"))
            return;
        var parts = line.split(" ", 3);
        map[parts[1]] = parts[2];
    });
    return map;
}

function dumpMacros(compilerFilePath, tag, nullDevice) {
    var map1 = dumpMcsCompilerMacros(compilerFilePath, tag, nullDevice);
    var map2 = dumpArmCompilerMacros(compilerFilePath, tag, nullDevice);
    var map = {};
    for (var key1 in map1)
        map[key1] = map1[key1];
    for (var key2 in map2)
        map[key2] = map2[key2];
    return map;
}

function dumpDefaultPaths(compilerFilePath, architecture) {
    var incDir = (isArmArchitecture(architecture)) ? "include" :  "inc";
    var includePath = compilerFilePath.replace(/bin[\\\/](.*)$/i, incDir);
    return {
        "includePaths": [includePath]
    };
}

function adjustPathsToWindowsSeparators(sourcePaths) {
    var resulingPaths = [];
    sourcePaths.forEach(function(path) {
        resulingPaths.push(FileInfo.toWindowsSeparators(path));
    });
    return resulingPaths;
}

function getMaxExitCode(architecture) {
    if (isMcsArchitecture(architecture))
        return 1;
    else if (isArmArchitecture(architecture))
        return 0;
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
        var sourceLines = output.split("\n");
        var filteredLines = [];
        for (var i in sourceLines) {
            var line = sourceLines[i];
            if (line.startsWith("***")
                || line.startsWith(">>")
                || line.startsWith("    ")
                || line.startsWith("  ACTION:")
                || line.startsWith("  LINE:")
                || line.startsWith("  ERROR:")
                || line.startsWith("Program Size:")
                || line.startsWith("A51 FATAL")
                || line.startsWith("C51 FATAL")
                || line.startsWith("C251 FATAL")
                || line.startsWith("ASSEMBLER INVOKED BY")
                || line.startsWith("LOC  OBJ            LINE     SOURCE")) {
                    filteredLines.push(sourceLines[i]);
            } else if (line.startsWith("C251 COMPILER")
                        || line.startsWith("C251 COMPILATION COMPLETE")
                        || line.startsWith("C251 TERMINATED")) {
                continue;
            } else {
                var regexp = /^([0-9A-F]{4})/;
                if (regexp.exec(line))
                    filteredLines.push(line);
            }
        }
        return filteredLines.join("\n");
    };
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
                  + (isMcsArchitecture(input.cpp.architecture)
                    ? input.fileName : input.baseName)
                  + ".lst"
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
                      product.targetName
                      + (product.cpp.architecture === "mcs51" ? ".m51" : ".map"))
    };
    return [app, mem_map];
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

    var allDefines = [];
    var platformDefines = input.cpp.platformDefines;
    if (platformDefines)
        allDefines = allDefines.uniqueConcat(platformDefines);
    var defines = input.cpp.defines;
    if (defines)
        allDefines = allDefines.uniqueConcat(defines);

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

    var architecture = input.qbs.architecture;
    if (isMcsArchitecture(architecture)) {
        // Input.
        args.push(FileInfo.toWindowsSeparators(input.filePath));

        // Output.
        args.push("OBJECT (" + FileInfo.toWindowsSeparators(outputs.obj[0].filePath) + ")");

        // Defines.
        if (allDefines.length > 0)
            args = args.concat("DEFINE (" + allDefines.join(",") + ")");

        // Includes.
        if (allIncludePaths.length > 0) {
            var adjusted = adjustPathsToWindowsSeparators(allIncludePaths);
            args = args.concat("INCDIR (" + adjusted.join(";") + ")");
        }

        // Debug information flags.
        if (input.cpp.debugInformation)
            args.push("DEBUG");

        // Optimization level flags.
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

        // Warning level flags.
        switch (input.cpp.warningLevel) {
        case "none":
            args.push("WARNINGLEVEL (0)");
            break;
        case "all":
            args.push("WARNINGLEVEL (2)");
            if (architecture === "mcs51")
                args.push("FARWARNING");
            break;
        }

        // Listing files generation flag.
        if (!product.cpp.generateCompilerListingFiles)
            args.push("NOPRINT");
        else
            args.push("PRINT(" + FileInfo.toWindowsSeparators(outputs.lst[0].filePath) + ")");
    } else if (isArmArchitecture(architecture)) {
        // Input.
        args.push("-c", input.filePath);

        // Output.
        args.push("-o", outputs.obj[0].filePath);

        // Defines.
        args = args.concat(allDefines.map(function(define) { return '-D' + define }));

        // Includes.
        args = args.concat(allIncludePaths.map(function(include) { return '-I' + include }));

        // Debug information flags.
        if (input.cpp.debugInformation) {
            args.push("--debug");
            args.push("-g");
        }

        // Optimization level flags.
        switch (input.cpp.optimization) {
        case "small":
            args.push("-Ospace")
            break;
        case "fast":
            args.push("-Otime")
            break;
        case "none":
            args.push("-O0")
            break;
        }

        // Warning level flags.
        switch (input.cpp.warningLevel) {
        case "none":
            args.push("-W");
            break;
        default:
            // By default all warnings are enabled.
            break;
        }

        if (tag === "c") {
            // C language version flags.
            var knownCLanguageValues = ["c99", "c90"];
            var cLanguageVersion = Cpp.languageVersion(
                        input.cpp.cLanguageVersion, knownCLanguageValues, "C");
            switch (cLanguageVersion) {
            case "c99":
                args.push("--c99");
                break;
            case "c90":
                args.push("--c90");
                break;
            }
        } else if (tag === "cpp") {
            // C++ language version flags.
            var knownCppLanguageValues = ["c++11", "c++03"];
            var cppLanguageVersion = Cpp.languageVersion(
                        input.cpp.cxxLanguageVersion, knownCppLanguageValues, "C++");
            switch (cppLanguageVersion) {
            case "c++11":
                args.push("--cpp11");
                break;
            default:
                // Default C++ language is C++03.
                args.push("--cpp");
                break;
            }

            // Exceptions flags.
            var enableExceptions = input.cpp.enableExceptions;
            if (enableExceptions !== undefined)
                args.push(enableExceptions ? "--exceptions" : "--no_exceptions");

            // RTTI flags.
            var enableRtti = input.cpp.enableRtti;
            if (enableRtti !== undefined)
                args.push(enableRtti ? "--rtti" : "--no_rtti");
        }

        // Listing files generation flag.
        if (product.cpp.generateCompilerListingFiles) {
            args.push("--list");
            args.push("--list_dir", FileInfo.path(outputs.lst[0].filePath));
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

function assemblerFlags(project, product, input, outputs, explicitlyDependsOn) {
    // Determine which C-language we're compiling
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(outputs.obj[0].fileTags));
    var args = [];

    var allDefines = [];
    var platformDefines = input.cpp.platformDefines;
    if (platformDefines)
        allDefines = allDefines.uniqueConcat(platformDefines);
    var defines = input.cpp.defines;
    if (defines)
        allDefines = allDefines.uniqueConcat(defines);

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

    var architecture = input.qbs.architecture;
    if (isMcsArchitecture(architecture)) {
        // Input.
        args.push(FileInfo.toWindowsSeparators(input.filePath));

        // Output.
        args.push("OBJECT (" + FileInfo.toWindowsSeparators(outputs.obj[0].filePath) + ")");

        // Defines.
        if (allDefines.length > 0)
            args = args.concat("DEFINE (" + allDefines.join(",") + ")");

        // Includes.
        if (allIncludePaths.length > 0) {
            var adjusted = adjustPathsToWindowsSeparators(allIncludePaths);
            args = args.concat("INCDIR (" + adjusted.join(";") + ")");
        }

        // Debug information flags.
        if (input.cpp.debugInformation)
            args.push("DEBUG");

        // Enable errors printing.
        args.push("EP");

        // Listing files generation flag.
        if (!product.cpp.generateAssemblerListingFiles)
            args.push("NOPRINT");
        else
            args.push("PRINT(" + FileInfo.toWindowsSeparators(outputs.lst[0].filePath) + ")");
    } else if (isArmArchitecture(architecture)) {
        // Input.
        args.push(input.filePath);

        // Output.
        args.push("-o", outputs.obj[0].filePath);

        // Defines.
        allDefines.forEach(function(define) {
            var parts = define.split("=");
            args.push("--pd");
            if (parts[1] === undefined)
                args.push(parts[0] + " SETA " + 1);
            else if (parts[1].contains("\""))
                args.push(parts[0] + " SETS " + parts[1]);
            else
                args.push(parts[0] + " SETA " + parts[1]);
        });

        // Includes.
        args = args.concat(allIncludePaths.map(function(include) { return '-I' + include }));

        // Debug information flags.
        if (input.cpp.debugInformation) {
            args.push("--debug");
            args.push("-g");
        }

        // Warning level flags.
        if (input.cpp.warningLevel === "none")
            args.push("--no_warn");

        // Byte order flags.
        var endianness = input.cpp.endianness;
        if (endianness)
            args.push((endianness === "little") ? "--littleend" : "--bigend");

        // Listing files generation flag.
        if (product.cpp.generateAssemblerListingFiles)
            args.push("--list", outputs.lst[0].filePath);
    }

    // Misc flags.
    args = args.concat(ModUtils.moduleProperty(input, "platformFlags", tag),
                       ModUtils.moduleProperty(input, "flags", tag),
                       ModUtils.moduleProperty(input, "driverFlags", tag));
    return args;
}

function linkerFlags(project, product, input, outputs) {
    var args = [];

    var architecture = product.qbs.architecture;
    if (isMcsArchitecture(architecture)) {
        // Note: The C51 linker does not distinguish an object files and
        // a libraries, it interpret all this stuff as an input objects,
        // so, we need to pass it together in one string.

        var allObjectPaths = [];
        function addObjectPath(obj) {
            allObjectPaths.push(obj.filePath);
        }

        // Inputs.
        if (inputs.obj)
            inputs.obj.map(function(obj) { addObjectPath(obj) });

        // Library dependencies.
        var libraryObjects = collectLibraryDependencies(product);
        libraryObjects.forEach(function(dep) { addObjectPath(dep); })

        // Add all input objects as arguments (application and library object files).
        var adjusted = adjustPathsToWindowsSeparators(allObjectPaths);
        args = args.concat(adjusted.join(","));

        // Output.
        // Note: We need to wrap an output file name with quotes. Otherwise
        // the linker will ignore a specified file name.
        args.push("TO", '"' + FileInfo.toWindowsSeparators(outputs.application[0].filePath) + '"');

        // Map file generation flag.
        if (!product.cpp.generateLinkerMapFile)
            args.push("NOMAP");
    } else if (isArmArchitecture(architecture)) {
        // Inputs.
        if (inputs.obj)
            args = args.concat(inputs.obj.map(function(obj) { return obj.filePath }));

        // Output.
        args.push("--output", outputs.application[0].filePath);

        // Library paths.
        var libraryPaths = product.cpp.libraryPaths;
        if (libraryPaths)
            args.push("--userlibpath=" + libraryPaths.join(","));

        // Library dependencies.
        var libraryDependencies = collectLibraryDependencies(product);
        args = args.concat(libraryDependencies.map(function(dep) { return dep.filePath; }));

        // Debug information flag.
        var debugInformation = product.cpp.debugInformation;
        if (debugInformation !== undefined)
            args.push(debugInformation ? "--debug" : "--no_debug");

        // Map file generation flag.
        if (product.cpp.generateLinkerMapFile)
            args.push("--list", outputs.mem_map[0].filePath);

        // Entry point flag.
        if (product.cpp.entryPoint)
            args.push("--entry", product.cpp.entryPoint);

        // Linker scripts flags.
        var linkerScripts = inputs.linkerscript
                ? inputs.linkerscript.map(function(a) { return a.filePath; }) : [];
        linkerScripts.forEach(function(script) { args.push("--scatter", script); });
    }

    // Misc flags.
    args = args.concat(ModUtils.moduleProperty(product, "driverLinkerFlags"));
    return args;
}

function archiverFlags(project, product, input, outputs) {
    var args = [];

    var architecture = product.qbs.architecture;
    if (isMcsArchitecture(architecture)) {
        // Library creation command.
        args.push("TRANSFER");

        var allObjectPaths = [];
        function addObjectPath(obj) {
            allObjectPaths.push(obj.filePath);
        }

        // Inputs.
        if (inputs.obj)
            inputs.obj.map(function(obj) { addObjectPath(obj) });

        // Add all input objects as arguments.
        var adjusted = adjustPathsToWindowsSeparators(allObjectPaths);
        args = args.concat(adjusted.join(","));

        // Output.
        // Note: We need to wrap a output file name with quotes. Otherwise
        // the linker will ignore a specified file name.
        args.push("TO", '"' + FileInfo.toWindowsSeparators(outputs.staticlibrary[0].filePath) + '"');
    } else if (isArmArchitecture(architecture)) {
        // Note: The ARM archiver command line expect the output file
        // first, and then a set of input objects.

        // Output.
        args.push("--create", outputs.staticlibrary[0].filePath);

        // Inputs.
        if (inputs.obj)
            args = args.concat(inputs.obj.map(function(obj) { return obj.filePath }));

        // Debug information flag.
        if (product.cpp.debugInformation)
            args.push("--debug_symbols");
    }

    return args;
}

function prepareCompiler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var args = compilerFlags(project, product, input, outputs, explicitlyDependsOn);
    var compilerPath = input.cpp.compilerPath;
    var architecture = input.cpp.architecture;
    var cmd = new Command(compilerPath, args);
    cmd.description = "compiling " + input.fileName;
    cmd.highlight = "compiler";
    cmd.maxExitCode = getMaxExitCode(architecture);
    filterStdOutput(cmd);
    return [cmd];
}

function prepareAssembler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var args = assemblerFlags(project, product, input, outputs, explicitlyDependsOn);
    var assemblerPath = input.cpp.assemblerPath;
    var cmd = new Command(assemblerPath, args);
    cmd.description = "assembling " + input.fileName;
    cmd.highlight = "compiler";
    filterStdOutput(cmd);
    return [cmd];
}

function prepareLinker(project, product, inputs, outputs, input, output) {
    var primaryOutput = outputs.application[0];
    var args = linkerFlags(project, product, input, outputs);
    var linkerPath = product.cpp.linkerPath;
    var architecture = product.cpp.architecture;
    var cmd = new Command(linkerPath, args);
    cmd.description = "linking " + primaryOutput.fileName;
    cmd.highlight = "linker";
    cmd.maxExitCode = getMaxExitCode(architecture);
    filterStdOutput(cmd);
    return [cmd];
}

function prepareArchiver(project, product, inputs, outputs, input, output) {
    var args = archiverFlags(project, product, input, outputs);
    var archiverPath = product.cpp.archiverPath;
    var cmd = new Command(archiverPath, args);
    cmd.description = "linking " + output.fileName;
    cmd.highlight = "linker";
    filterStdOutput(cmd);
    return [cmd];
}
