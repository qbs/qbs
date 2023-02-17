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

var BinaryFile = require("qbs.BinaryFile");
var Cpp = require("cpp.js");
var Environment = require("qbs.Environment");
var File = require("qbs.File");
var FileInfo = require("qbs.FileInfo");
var Host = require("qbs.Host");
var ModUtils = require("qbs.ModUtils");
var PathTools = require("qbs.PathTools");
var Process = require("qbs.Process");
var TemporaryDir = require("qbs.TemporaryDir");
var TextFile = require("qbs.TextFile");
var Utilities = require("qbs.Utilities");
var WindowsUtils = require("qbs.WindowsUtils");

function toolchainDetails(qbs) {
    var architecture = qbs.architecture;
    if (architecture === "mcs51") {
        return {
            "assemblerName": "sdas8051",
            "linkerName": "sdld"
        }
    } else if (architecture === "stm8") {
        return {
            "assemblerName": "sdasstm8",
            "linkerName": "sdldstm8"
        }
    } else if (architecture === "hcs8") {
        return {
            "assemblerName": "sdas6808",
            "linkerName": "sdld6808"
        }
    }
}

function targetArchitectureFlag(architecture) {
    if (architecture === "mcs51")
        return "-mmcs51";
    if (architecture === "stm8")
        return "-mstm8";
    if (architecture === "hcs8")
        return "-mhc08";
}

function guessArchitecture(macros) {
    if (macros["__SDCC_mcs51"] === "1")
        return "mcs51";
    if (macros["__SDCC_stm8"] === "1")
        return "stm8";
    if (macros["__SDCC_hc08"] === "1")
        return "hcs8";
}

function guessEndianness(macros) {
    // SDCC stores numbers in little-endian format.
    return "little";
}

function guessVersion(macros) {
    if ("__SDCC_VERSION_MAJOR" in macros
            && "__SDCC_VERSION_MINOR" in macros
            && "__SDCC_VERSION_PATCH" in macros) {
        return { major: parseInt(macros["__SDCC_VERSION_MAJOR"], 10),
            minor: parseInt(macros["__SDCC_VERSION_MINOR"], 10),
            patch: parseInt(macros["__SDCC_VERSION_PATCH"], 10) }
    } else if ("__SDCC" in macros) {
        var versions = macros["__SDCC"].split("_");
        if (versions.length === 3) {
            return { major: parseInt(versions[0], 10),
                minor: parseInt(versions[1], 10),
                patch: parseInt(versions[2], 10) };
        }
    }
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
    return Cpp.extractMacros(p.readStdOut());
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
            if (File.exists(line))
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

function escapePreprocessorFlags(preprocessorFlags) {
    if (!preprocessorFlags || preprocessorFlags.length === 0)
        return preprocessorFlags;
    return ["-Wp " + preprocessorFlags.join(",")];
}

// We need to use the asm_adb, asm_src, asm_sym and rst_data
// artifacts without of any conditions. Because SDCC always generates
// it (and seems, this behavior can not be disabled for SDCC).
function extraCompilerOutputTags() {
    return ["asm_adb", "asm_src", "asm_sym", "rst_data"];
}

// We need to use the lk_cmd, and mem_summary artifacts without
// of any conditions. Because SDCC always generates
// it (and seems, this behavior can not be disabled for SDCC).
function extraApplicationLinkerOutputTags() {
    return ["lk_cmd", "mem_summary"];
}

// We need to use the asm_adb, asm_src, asm_sym and rst_data
// artifacts without of any conditions. Because SDCC always generates
// it (and seems, this behavior can not be disabled for SDCC).
function extraCompilerOutputArtifacts(input) {
    return [{
        fileTags: ["asm_adb"],
        filePath: Utilities.getHash(input.baseDir) + "/"
            + input.fileName + ".adb"
    }, {
        fileTags: ["asm_src"],
        filePath: Utilities.getHash(input.baseDir) + "/"
            + input.fileName + ".asm"
    }, {
        fileTags: ["asm_sym"],
        filePath: Utilities.getHash(input.baseDir) + "/"
            + input.fileName + ".sym"
    }, {
        fileTags: ["rst_data"],
        filePath: Utilities.getHash(input.baseDir) + "/"
            + input.fileName + ".rst"
    }];
}

// We need to use the lk_cmd, and mem_summary artifacts without
// of any conditions. Because SDCC always generates
// it (and seems, this behavior can not be disabled for SDCC).
function extraApplicationLinkerOutputArtifacts(product) {
    return [{
        fileTags: ["lk_cmd"],
        filePath: FileInfo.joinPaths(
            product.destinationDirectory,
            product.targetName + ".lk")
    }, {
        fileTags: ["mem_summary"],
        filePath: FileInfo.joinPaths(
            product.destinationDirectory,
            product.targetName + ".mem")
    }];
}

function compilerFlags(project, product, input, outputs, explicitlyDependsOn) {
    var args = [];
    var escapablePreprocessorFlags = [];

    // Input.
    args.push(input.filePath);

    // Output.
    args.push("-c");
    args.push("-o", outputs.obj[0].filePath);

    // Prefix headers.
    escapablePreprocessorFlags = escapablePreprocessorFlags.concat(
                Cpp.collectPreincludePathsArguments(input));

    // Defines.
    args = args.concat(Cpp.collectDefinesArguments(input));

    // Includes.
    args = args.concat(Cpp.collectIncludePathsArguments(input));
    escapablePreprocessorFlags = escapablePreprocessorFlags.concat(
                Cpp.collectSystemIncludePathsArguments(input));

    // Target MCU flag.
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
    var warnings = input.cpp.warningLevel
    if (warnings === "none") {
        args.push("--less-pedantic");
        escapablePreprocessorFlags.push("-w");
    } else if (warnings === "all") {
        escapablePreprocessorFlags.push("-Wall");
    }
    if (input.cpp.treatWarningsAsErrors)
        args.push("--Werror");

    // Determine which C-language we"re compiling.
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(outputs.obj[0].fileTags));

    // C language version flags.
    if (tag === "c") {
        var knownValues = ["c2x", "c17", "c11", "c99", "c89"];
        var cLanguageVersion = Cpp.languageVersion(
                    input.cpp.cLanguageVersion, knownValues, "C");
        switch (cLanguageVersion) {
        case "c17":
            cLanguageVersion = "c11";
            // fall through
        case "c89":
        case "c99":
        case "c11":
        case "c2x":
            args.push("--std-" + cLanguageVersion);
            break;
        }
    }

    var escapedPreprocessorFlags = escapePreprocessorFlags(escapablePreprocessorFlags);
    if (escapedPreprocessorFlags)
        Array.prototype.push.apply(args, escapedPreprocessorFlags);

    // Misc flags.
    args = args.concat(Cpp.collectMiscCompilerArguments(input, tag),
                       Cpp.collectMiscDriverArguments(input));
    return args;
}

function assemblerFlags(project, product, input, outputs, explicitlyDependsOn) {
    var args = [];

    // Includes.
    args = args.concat(Cpp.collectIncludePathsArguments(input));
    args = args.concat(Cpp.collectSystemIncludePathsArguments(input, input.cpp.includeFlag));

    // Misc flags.
    args = args.concat(Cpp.collectMiscAssemblerArguments(input, "asm"));

    args.push("-ol");
    args.push(outputs.obj[0].filePath);
    args.push(input.filePath);
    return args;
}

function linkerFlags(project, product, inputs, outputs) {
    var args = [];

    // Target MCU flag.
    var targetFlag = targetArchitectureFlag(product.cpp.architecture);
    if (targetFlag)
        args.push(targetFlag);

    var escapableLinkerFlags = [];

    // Map file generation flag.
    if (product.cpp.generateLinkerMapFile)
        escapableLinkerFlags.push("-m");

    escapableLinkerFlags = escapableLinkerFlags.concat(Cpp.collectMiscEscapableLinkerArguments(product));

    var useCompilerDriver = useCompilerDriverLinker(product);

    // Output.
    if (useCompilerDriver)
        args.push("-o");
    args.push(outputs.application[0].filePath);

    // Inputs.
    args = args.concat(Cpp.collectLinkerObjectPaths(inputs));

    // Library paths.
    var libraryPathFlag = useCompilerDriver ? "-L" : "-k";
    args = args.concat(Cpp.collectLibraryPathsArguments(product, libraryPathFlag));

    // Linker scripts.
    // Note: We need to split the '-f' flag and the file path to separate
    // lines when we don't use the compiler driver mode.
    escapableLinkerFlags = escapableLinkerFlags.concat(
                Cpp.collectLinkerScriptPathsArguments(product, inputs, !useCompilerDriver));

    // Library dependencies.
    args = args.concat(Cpp.collectLibraryDependenciesArguments(product));

    var escapedLinkerFlags = escapeLinkerFlags(product, escapableLinkerFlags);
    if (escapedLinkerFlags)
        Array.prototype.push.apply(args, escapedLinkerFlags);

    // Misc flags.
    if (useCompilerDriver) {
        args = args.concat(Cpp.collectMiscLinkerArguments(product),
                           Cpp.collectMiscDriverArguments(product));
    }
    return args;
}

function archiverFlags(project, product, inputs, outputs) {
    var args = ["-rc"];
    args.push(outputs.staticlibrary[0].filePath);
    args = args.concat(Cpp.collectLinkerObjectPaths(inputs));
    return args;
}

function buildLinkerMapFilePath(target, suffix) {
    return FileInfo.joinPaths(FileInfo.path(target.filePath),
                              FileInfo.completeBaseName(target.fileName) + suffix);
}

// This is the workaround for the SDCC bug on a Windows host:
// * https://sourceforge.net/p/sdcc/bugs/2970/
// We need to replace the '\r\n\' line endings with the'\n' line
// endings for each generated object file.
function patchObjectFile(project, product, inputs, outputs, input, output) {
    var isWindows = Host.os().includes("windows");
    if (isWindows && input.cpp.debugInformation) {
        var cmd = new JavaScriptCommand();
        cmd.objectPath = outputs.obj[0].filePath;
        cmd.silent = true;
        cmd.sourceCode = function() {
            var file = new BinaryFile(objectPath, BinaryFile.ReadWrite);
            var data = file.read(file.size());
            file.resize(0);
            for (var pos = 0; pos < data.length; ++pos) {
                // Find the next index of CR (\r) symbol.
                var index = data.indexOf(0x0d, pos);
                if (index < 0)
                    index = data.length;
                // Write next data chunk between the previous position and the CR
                // symbol, exclude the CR symbol.
                file.write(data.slice(pos, index));
                pos = index;
            }
        };
        return cmd;
    }
}

// It is a workaround which removes the generated linker map file
// if it is disabled by cpp.generateLinkerMapFile property.
// Reason is that the SDCC compiler always generates this file,
// and does not have an option to disable generation for a linker
// map file. So, we can to remove a listing files only after the
// linking completes.
function removeLinkerMapFile(project, product, inputs, outputs, input, output) {
    if (!product.cpp.generateLinkerMapFile) {
        var target = outputs.application[0];
        var cmd = new JavaScriptCommand();
        cmd.mapFilePath = buildLinkerMapFilePath(target, product.cpp.linkerMapSuffix)
        cmd.silent = true;
        cmd.sourceCode = function() { File.remove(mapFilePath); };
        return cmd;
    }
}

// It is a workaround to rename the extension of the output linker
// map file to the specified one, since the linker generates only
// files with the '.map' extension.
function renameLinkerMapFile(project, product, inputs, outputs, input, output) {
    if (product.cpp.generateLinkerMapFile && (product.cpp.linkerMapSuffix !== ".map")) {
        var target = outputs.application[0];
        var cmd = new JavaScriptCommand();
        cmd.newMapFilePath = buildLinkerMapFilePath(target, product.cpp.linkerMapSuffix);
        cmd.oldMapFilePath = buildLinkerMapFilePath(target, ".map");
        cmd.silent = true;
        cmd.sourceCode = function() { File.move(oldMapFilePath, newMapFilePath); };
        return cmd;
    }
}

// It is a workaround which removes the generated listing files
// if it is disabled by cpp.generateCompilerListingFiles property
// or when the cpp.compilerListingSuffix differs with '.lst'.
// Reason is that the SDCC compiler does not have an option to
// disable generation for a listing files. Besides, the SDCC
// compiler use this files and for the linking. So, we can to
// remove a listing files only after the linking completes.
function removeCompilerListingFiles(project, product, inputs, outputs, input, output) {
    var cmd = new JavaScriptCommand();
    cmd.silent = true;
    cmd.sourceCode = function() {
        inputs.obj.forEach(function(object) {
            if (!object.filePath.endsWith(".c" + object.cpp.objectSuffix))
                return; // Skip the assembler generated objects.
            if (!object.cpp.generateCompilerListingFiles
                    || (object.cpp.compilerListingSuffix !== ".lst")) {
                var listingPath = FileInfo.joinPaths(FileInfo.path(object.filePath),
                                                     object.completeBaseName + ".lst");
                File.remove(listingPath);
            }
        })
    };
    return cmd;
}

// It is a workaround that duplicates the generated listing files
// but with desired names. The problem is that the SDCC compiler does
// not support an options to specify names for the generated listing
// files. At the same time, the compiler always generates the listing
// files in the form of 'module.c.lst', which makes it impossible to
// change the file suffix to a user-specified one. In addition, these
// files are also somehow used for linking. Thus, we can not rename them
// on the compiling stage.
function duplicateCompilerListingFile(project, product, inputs, outputs, input, output) {
    if (input.cpp.generateCompilerListingFiles
            && (input.cpp.compilerListingSuffix !== ".lst")) {
        var cmd = new JavaScriptCommand();
        cmd.newListing = outputs.lst[0].filePath;
        cmd.oldListing = FileInfo.joinPaths(FileInfo.path(outputs.lst[0].filePath),
                                            outputs.lst[0].completeBaseName + ".lst");
        cmd.silent = true;
        cmd.sourceCode = function() { File.copy(oldListing, newListing); };
        return cmd;
    }
}

function prepareCompiler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var cmds = [];
    var args = compilerFlags(project, product, input, outputs, explicitlyDependsOn);
    var compilerPath = input.cpp.compilerPath;
    var cmd = new Command(compilerPath, args);
    cmd.description = "compiling " + input.fileName;
    cmd.highlight = "compiler";
    cmd.jobPool = "compiler";
    cmds.push(cmd);

    cmd = patchObjectFile(project, product, inputs, outputs, input, output);
    if (cmd)
        cmds.push(cmd);

    cmd = duplicateCompilerListingFile(project, product, inputs, outputs, input, output);
    if (cmd)
        cmds.push(cmd);

    return cmds;
}

function prepareAssembler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var cmds = [];
    var args = assemblerFlags(project, product, input, outputs, explicitlyDependsOn);
    var assemblerPath = input.cpp.assemblerPath;
    var cmd = new Command(assemblerPath, args);
    cmd.description = "assembling " + input.fileName;
    cmd.highlight = "compiler";
    cmd.jobPool = "assembler";
    cmds.push(cmd);

    cmd = patchObjectFile(project, product, inputs, outputs, input, output);
    if (cmd)
        cmds.push(cmd);

    return cmds;
}

function prepareLinker(project, product, inputs, outputs, input, output) {
    var cmds = [];
    var args = linkerFlags(project, product, inputs, outputs);
    var linkerPath = effectiveLinkerPath(product);
    var cmd = new Command(linkerPath, args);
    cmd.description = "linking " + outputs.application[0].fileName;
    cmd.highlight = "linker";
    cmd.jobPool = "linker";
    cmds.push(cmd);

    cmd = removeCompilerListingFiles(project, product, inputs, outputs, input, output);
    if (cmd)
        cmds.push(cmd);

    cmd = renameLinkerMapFile(project, product, inputs, outputs, input, output);
    if (cmd)
        cmds.push(cmd);

    cmd = removeLinkerMapFile(project, product, inputs, outputs, input, output);
    if (cmd)
        cmds.push(cmd);

    return cmds;
}

function prepareArchiver(project, product, inputs, outputs, input, output) {
    var args = archiverFlags(project, product, inputs, outputs);
    var archiverPath = product.cpp.archiverPath;
    var cmd = new Command(archiverPath, args);
    cmd.description = "creating " + output.fileName;
    cmd.highlight = "linker";
    cmd.jobPool = "linker";
    return [cmd];
}
