/****************************************************************************
**
** Copyright (C) 2021 Denis Shienkov <denis.shienkov@gmail.com>
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

function toolchainDetails(qbs) {
    var architecture = qbs.architecture;
    if (architecture.startsWith("arm")) {
        return {
            "executableSuffix": ".cxm",
            "staticLibrarySuffix": ".cxm",
            "assemblerName": "cacorm",
            "compilerName": "cxcorm"
        };
    } else if (architecture === "stm8") {
        return {
            "executableSuffix": ".sm8",
            "staticLibrarySuffix": ".sm8",
            "assemblerName": "castm8",
            "compilerName": "cxstm8"
        };
    } else if (architecture === "hcs8") {
        return {
            "executableSuffix": ".h08",
            "staticLibrarySuffix": ".h08",
            "assemblerName": "ca6808",
            "compilerName": "cx6808"
        };
    } else if (architecture === "hcs12") {
        return {
            "executableSuffix": ".h12",
            "staticLibrarySuffix": ".h12",
            "assemblerName": "ca6812",
            "compilerName": "cx6812"
        };
    } else if (architecture === "m68k") {
        return {
            "executableSuffix": ".332",
            "staticLibrarySuffix": ".332",
            "assemblerName": "ca332",
            "compilerName": "cx332"
        };
    }
}

function guessArchitecture(compilerFilePath) {
    var baseName = FileInfo.baseName(compilerFilePath);
    if (baseName === "cxcorm")
        return "arm";
    else if (baseName === "cxstm8")
        return "stm8";
    else if (baseName === "cx6808")
        return "hcs8";
    else if (baseName === "cx6812")
        return "hcs12";
    else if (baseName === "cx332")
        return "m68k";
}

function dumpMacros(compilerFilePath) {
    // Note: The COSMIC compiler does not support the predefined
    // macros dumping. So, we do it with the following trick, where we try
    // to create and compile a special temporary file and to parse the console
    // output with the own magic pattern: (""|"key"|"value"|"").

    var outputDirectory = new TemporaryDir();
    var outputFilePath = FileInfo.fromNativeSeparators(FileInfo.joinPaths(outputDirectory.path(),
                                                       "dump-macros.c"));
    var outputFile = new TextFile(outputFilePath, TextFile.WriteOnly);
    outputFile.writeLine("#define VALUE_TO_STRING(x) #x");
    outputFile.writeLine("#define VALUE(x) VALUE_TO_STRING(x)");
    outputFile.writeLine("#define VAR_NAME_VALUE(var) #var VALUE(var)");
    // The COSMIC compiler defines only one pre-defined macro
    // (at least nothing is said about other macros in the documentation).
    var keys = ["__CSMC__"];
    for (var i in keys) {
        var key = keys[i];
        outputFile.writeLine("#if defined (" + key + ")");
        outputFile.writeLine("#pragma message (VAR_NAME_VALUE(" + key + "))");
        outputFile.writeLine("#endif");
    }
    outputFile.close();

    var process = new Process();
    process.exec(compilerFilePath, [outputFilePath], false);
    File.remove(outputFilePath);

    var map = {};
    // COSMIC compiler use the errors output!
    process.readStdErr().trim().split(/\r?\n/g).map(function(line) {
        var match = line.match(/^#message \("(.+)" "(.+)"\)$/);
        if (match)
            map[match[1]] = match[2];
    });
    return map;
}

function dumpVersion(compilerFilePath) {
    var p = new Process();
    p.exec(compilerFilePath, ["-vers"]);
    // COSMIC compiler use the errors output!
    var output = p.readStdErr();
    var match = output.match(/^COSMIC.+V(\d+)\.?(\d+)\.?(\*|\d+)?/);
    if (match) {
        var major = match[1] ? parseInt(match[1], 10) : 0;
        var minor = match[2] ? parseInt(match[2], 10) : 0;
        var patch = match[3] ? parseInt(match[3], 10) : 0;
        return { major: major, minor: minor, patch: patch };
    }
}

function guessEndianness(architecture) {
    // There is no mention of supported endianness in the cosmic compiler.
    return "big";
}

function dumpDefaultPaths(compilerFilePath, architecture) {
    var rootPath = FileInfo.path(compilerFilePath);
    var includePath;
    var includePaths = [];
    if (architecture.startsWith("arm")) {
        includePath = FileInfo.joinPaths(rootPath, "hcorm");
        if (File.exists(includePath))
            includePaths.push(includePath);
    } else if (architecture === "stm8") {
        includePath = FileInfo.joinPaths(rootPath, "hstm8");
        if (File.exists(includePath))
            includePaths.push(includePath);
    } else if (architecture === "hcs8") {
        includePath = FileInfo.joinPaths(rootPath, "h6808");
        if (File.exists(includePath))
            includePaths.push(includePath);
    } else if (architecture === "hcs12") {
        includePath = FileInfo.joinPaths(rootPath, "h6812");
        if (File.exists(includePath))
            includePaths.push(includePath);
    } else if (architecture === "m68k") {
        includePath = FileInfo.joinPaths(rootPath, "h332");
        if (File.exists(includePath))
            includePaths.push(includePath);
    }

    var libraryPaths = [];
    var libraryPath = FileInfo.joinPaths(rootPath, "lib");
    if (File.exists(libraryPath))
        libraryPaths.push(libraryPath);

    return {
        "includePaths": includePaths,
        "libraryPaths": libraryPaths,
    }
}

function compilerFlags(project, product, input, outputs, explicitlyDependsOn) {
    var args = [];

    // Up to 128 include files.
    args = args.concat(Cpp.collectPreincludePathsArguments(input));

    // Defines.
    args = args.concat(Cpp.collectDefinesArguments(input));

    // Up to 128 include paths.
    args = args.concat(Cpp.collectIncludePathsArguments(input));
    args = args.concat(Cpp.collectSystemIncludePathsArguments(input));

    // Debug information flags.
    if (input.cpp.debugInformation)
        args.push("+debug");

    var architecture = input.qbs.architecture;
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(outputs.obj[0].fileTags));

    // Warning level flags.
    switch (input.cpp.warningLevel) {
    case "none":
        // Disabled by default.
        break;
    case "all":
        // Highest warning level.
        args.push("-pw7");
        break;
    }

    // C language version flags.
    if (tag === "c") {
        var knownValues = ["c99"];
        var cLanguageVersion = Cpp.languageVersion(
                    input.cpp.cLanguageVersion, knownValues, "C");
        switch (cLanguageVersion) {
        case "c99":
            args.push("-p", "c99");
            break;
        default:
            break;
        }
    }

    // Objects output directory.
    args.push("-co", FileInfo.path(outputs.obj[0].filePath));

    // Listing files generation flag.
    if (input.cpp.generateCompilerListingFiles) {
        // Enable listings.
        args.push("-l");
        // Listings output directory.
        args.push("-cl", FileInfo.path(outputs.lst[0].filePath));
    }

    // Misc flags.
    args = args.concat(Cpp.collectMiscCompilerArguments(input, tag),
                       Cpp.collectMiscDriverArguments(product));

    // Input.
    args.push(input.filePath);
    return args;
}

function assemblerFlags(project, product, input, outputs, explicitlyDependsOn) {
    var args = [];

    // Up to 128 include paths.
    args = args.concat(Cpp.collectIncludePathsArguments(input));
    args = args.concat(Cpp.collectSystemIncludePathsArguments(input));

    // Debug information flags.
    if (input.cpp.debugInformation)
        args.push("-xx");

    // Misc flags.
    args = args.concat(Cpp.collectMiscAssemblerArguments(input, "asm"));

    // Listing files generation flag.
    if (input.cpp.generateAssemblerListingFiles) {
        args.push("-l");
        args.push("+l", outputs.lst[0].filePath);
    }

    // Objects output file path.
    args.push("-o", outputs.obj[0].filePath);

    // Input.
    args.push(input.filePath);
    return args;
}

function linkerFlags(project, product, inputs, outputs) {
    var args = [];

    // Library paths.
    args = args.concat(Cpp.collectLibraryPaths(product).map(function(path) {
        // It is workaround to use the relative paths avoids a strange linking
        // errors. Maybe it is related to the limitations on the length of the
        // command arguments, or on the length of the paths.
        return product.cpp.libraryPathFlag + Cpp.relativePath(product.buildDirectory, path);
    }));

    // Output.
    args.push("-o", outputs.application[0].filePath);

    // Map file generation flag.
    if (product.cpp.generateLinkerMapFile)
        args.push("-m", outputs.mem_map[0].filePath);

    // Misc flags.
    args = args.concat(Cpp.collectMiscEscapableLinkerArguments(product),
                       Cpp.collectMiscLinkerArguments(product),
                       Cpp.collectMiscDriverArguments(product));

    // Linker scripts.
    args = args.concat(Cpp.collectLinkerScriptPathsArguments(product, inputs));

    // Input objects.
    args = args.concat(Cpp.collectLinkerObjectPaths(inputs));

    // Library dependencies (order has matters).
    args = args.concat(Cpp.collectLibraryDependencies(product).map(function(dep) {
        // It is workaround to use the relative paths avoids a strange linking
        // errors. Maybe it is related to the limitations on the length of the
        // command arguments, or on the length of the paths.
        return Cpp.relativePath(product.buildDirectory, dep.filePath);
    }));

    return args;
}

function archiverFlags(project, product, inputs, outputs) {
    var args = ["-cl"];

    // Output.
    args.push(outputs.staticlibrary[0].filePath);

    // Input objects.
    args = args.concat(Cpp.collectLinkerObjectPaths(inputs));
    return args;
}

function createPath(fullPath) {
    var cmd = new JavaScriptCommand();
    cmd.fullPath = fullPath;
    cmd.silent = true;
    cmd.sourceCode = function() {
        File.makePath(fullPath);
    };
    return cmd;
}

// It is a workaround to rename the generated object file to the desired name.
// Reason is that the Cosmic compiler always generates the object files in the
// format of 'module.o', but we expect it in flexible format, e.g. 'module.c.obj'
// or 'module.c.o' depending on the cpp.objectSuffix property.
function renameObjectFile(project, product, inputs, outputs, input, output) {
    var object = outputs.obj[0];
    var cmd = new JavaScriptCommand();
    cmd.newObject = object.filePath;
    cmd.oldObject = FileInfo.joinPaths(FileInfo.path(object.filePath),
                                       object.baseName + ".o");
    cmd.silent = true;
    cmd.sourceCode = function() { File.move(oldObject, newObject); };
    return cmd;
}

// It is a workaround to rename the generated listing file to the desired name.
// Reason is that the Cosmic compiler always generates the listing files in the
// format of 'module.ls', but we expect it in flexible format, e.g. 'module.c.lst'
// or 'module.c.ls' depending on the cpp.compilerListingSuffix property.
function renameListingFile(project, product, inputs, outputs, input, output) {
    var listing = outputs.lst[0];
    var cmd = new JavaScriptCommand();
    cmd.newListing = listing.filePath;
    cmd.oldListing = FileInfo.joinPaths(FileInfo.path(listing.filePath),
                                        listing.baseName + ".ls");
    cmd.silent = true;
    cmd.sourceCode = function() { File.move(oldListing, newListing); };
    return cmd;
}

function prepareCompiler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var cmds = [];

    // Create output objects path, because the Cosmic doesn't do it.
    var cmd = createPath(FileInfo.path(outputs.obj[0].filePath));
    cmds.push(cmd);

    // Create output listing path, because the Cosmic doesn't do it.
    if (input.cpp.generateCompilerListingFiles) {
        cmd = createPath(FileInfo.path(outputs.lst[0].filePath));
        cmds.push(cmd);
    }

    var args = compilerFlags(project, product, input, outputs, explicitlyDependsOn);
    cmd = new Command(input.cpp.compilerPath, args);
    cmd.workingDirectory = product.buildDirectory;
    cmd.description = "compiling " + input.fileName;
    cmd.highlight = "compiler";
    cmd.jobPool = "compiler";
    cmds.push(cmd);

    cmds.push(renameObjectFile(project, product, inputs, outputs, input, output));

    if (input.cpp.generateCompilerListingFiles)
        cmds.push(renameListingFile(project, product, inputs, outputs, input, output));

    return cmds;
}

function prepareAssembler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var cmds = [];

    // Create output objects path, because the Cosmic doesn't do it.
    var cmd = createPath(FileInfo.path(outputs.obj[0].filePath));
    cmds.push(cmd);

    // Create output listing path, because the Cosmic doesn't do it.
    if (input.cpp.generateCompilerListingFiles) {
        cmd = createPath(FileInfo.path(outputs.lst[0].filePath));
        cmds.push(cmd);
    }

    var args = assemblerFlags(project, product, input, outputs, explicitlyDependsOn);
    cmd = new Command(input.cpp.assemblerPath, args);
    cmd.workingDirectory = product.buildDirectory;
    cmd.description = "assembling " + input.fileName;
    cmd.highlight = "compiler";
    cmd.jobPool = "assembler";
    cmds.push(cmd);
    return cmds;
}

function prepareLinker(project, product, inputs, outputs, input, output) {
    var primaryOutput = outputs.application[0];
    var args = linkerFlags(project, product, inputs, outputs);
    var cmd = new Command(product.cpp.linkerPath, args);
    cmd.workingDirectory = product.buildDirectory;
    cmd.description = "linking " + primaryOutput.fileName;
    cmd.highlight = "linker";
    cmd.jobPool = "linker";
    return [cmd];
}

function prepareArchiver(project, product, inputs, outputs, input, output) {
    var args = archiverFlags(project, product, inputs, outputs);
    var cmd = new Command(product.cpp.archiverPath, args);
    cmd.workingDirectory = product.buildDirectory;
    cmd.description = "creating " + output.fileName;
    cmd.highlight = "linker";
    cmd.jobPool = "linker";
    return [cmd];
}
