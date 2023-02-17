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
var Utilities = require("qbs.Utilities");

function targetFlags(platform, architecture, extender, consoleApp, type) {
    if (platform === "dos") {
        if (architecture === "x86_16") {
            if (extender === "dosz")
                return ["-mz"];
            else if (extender === "dosr")
                return ["-mr"];
            return ["-mc"];
        } else if (architecture === "x86") {
            if (extender === "dosx")
                return ["-mx"];
            else if (extender === "dosp")
                return ["-mp"];
        }
    } else if (platform === "windows") {
        var flags = [];
        if (architecture === "x86_16") {
            flags.push("-ml");
            if (type.includes("application") && !consoleApp)
                flags.push("-WA");
            else if (type.includes("dynamiclibrary"))
                flags.push("-WD");
        } else if (architecture === "x86") {
            flags.push("-mn");
            if (type.includes("application"))
                flags.push("-WA");
            else if (type.includes("dynamiclibrary"))
                flags.push("-WD");
        }
        return flags;
    }
    return [];
}

function languageFlags(tag) {
    if (tag === "cpp")
        return ["-cpp"];
    return [];
}

function dumpMacros(compilerPath, platform, architecture, extender, tag) {
    // Note: The DMC compiler does not support the predefined/ macros dumping. So, we do it
    // with the following trick, where we try to create and compile a special temporary file
    // and to parse the console output with the own magic pattern: #define <key> <value>.

    var outputDirectory = new TemporaryDir();
    var outputFilePath = FileInfo.joinPaths(outputDirectory.path(), "dump-macros.c");
    var outputFile = new TextFile(outputFilePath, TextFile.WriteOnly);
    outputFile.writeLine("#define VALUE_TO_STRING(x) #x");
    outputFile.writeLine("#define VALUE(x) VALUE_TO_STRING(x)");
    outputFile.writeLine("#define VAR_NAME_VALUE(var) \"#define \" #var\" \"VALUE(var)");
    // Declare all available pre-defined macros of DMC compiler.
    var keys = [
                // Prepare the DOS target macros.
                "_MSDOS", "MSDOS",
                // Prepare the OS/2 target macros.
                "__OS2__",
                // Prepare the Windows target macros.
                "WIN32", "_WIN32", "__NT__",
                // Prepare extended the 32 and 16 bit DOS target macros.
                "DOS386", "DOS16RM",
                // Prepare the memory model macros.
                "M_I86", "_M_I86",
                "_M_I86TM", "M_I86TM",
                "_M_I86SM", "M_I86SM",
                "_M_I86MM", "M_I86MM",
                "_M_I86CM", "M_I86CM",
                "_M_I86LM", "M_I86LM",
                "_M_I86VM", "M_I86VM",
                // Prepare 8086 macros.
                "_M_I8086", "M_I8086",
                // Prepare 286 macros.
                "_M_I286", "M_I286",
                // Prepare 32 bit macros.
                "_M_IX86",
                // Prepare compiler identification macros.
                "__DMC__", "__DMC_VERSION_STRING__",
                // Prepare common compiler macros.
                "_CHAR_UNSIGNED", "_CHAR_EQ_UCHAR", "_DEBUG_TRACE", "_DLL",
                "_ENABLE_ARRAYNEW", "_BOOL_DEFINED", "_WCHAR_T_DEFINED",
                "_CPPRTTI", "_CPPUNWIND", "_MD", "_PUSHPOP_SUPPORTED",
                "_STDCALL_SUPPORTED", "__INTSIZE", "__DEFALIGN", "_INTEGRAL_MAX_BITS",
                "_WINDOWS", "_WINDLL", "__INLINE_8087", "__I86__", "__SMALL__",
                "__MEDIUM__", "__COMPACT__", "__LARGE__", "__VCM__", "__FPCE__",
                "__FPCE__IEEE__", "DEBUG",
                // Prepare C99 and C++98 macros.
                "__STDC__", "__STDC_HOSTED__", "__STDC_VERSION__", "__STDC_IEC_559__",
                "__STDC_IEC_559_COMPLEX__", "__STDC_ISO_10646__", "__cplusplus"
            ];
    for (var i = 0; i < keys.length; ++i) {
        var key = keys[i];
        outputFile.writeLine("#if defined (" + key + ")");
        outputFile.writeLine("#pragma message (VAR_NAME_VALUE(" + key + "))");
        outputFile.writeLine("#endif");
    }
    outputFile.close();

    var process = new Process();
    process.setWorkingDirectory(outputDirectory.path());
    var lang = languageFlags(tag);
    var target = targetFlags(platform, architecture, extender, false, ["application"]);
    var args = ["-c"].concat(lang, target, FileInfo.toWindowsSeparators(outputFilePath));
    process.exec(compilerPath, args, false);
    File.remove(outputFilePath);
    var out = process.readStdOut();
    return Cpp.extractMacros(out);
}

function dumpDefaultPaths(compilerFilePath, tag) {
    var binPath = FileInfo.path(compilerFilePath);
    var rootPath = FileInfo.path(binPath);
    var includePaths = [];
    var cppIncludePath = FileInfo.joinPaths(rootPath, "stlport/stlport");
    if (File.exists(cppIncludePath))
        includePaths.push(cppIncludePath);
    var cIncludePath = FileInfo.joinPaths(rootPath, "include");
    if (File.exists(cIncludePath))
        includePaths.push(cIncludePath);

    var libraryPaths = [];
    var libraryPath = FileInfo.joinPaths(rootPath, "lib");
    if (File.exists(libraryPath))
        libraryPaths.push(libraryPath);

    return {
        "includePaths": includePaths,
        "libraryPaths": libraryPaths,
    }
}

function guessVersion(macros) {
    var version = macros["__DMC__"];
    return { major: parseInt(version / 100),
        minor: parseInt(version % 100),
        patch: 0 };
}

function effectiveLinkerPath(product) {
    if (product.cpp.linkerMode === "automatic") {
        var compilerPath = product.cpp.compilerPath;
        if (compilerPath)
            return compilerPath;
        console.log("Found no C-language objects, choosing system linker for " + product.name);
    }
    return product.cpp.linkerPath;
}

function useCompilerDriverLinker(product) {
    var linker = effectiveLinkerPath(product);
    var compilers = product.cpp.compilerPathByLanguage;
    if (compilers)
        return linker === compilers["cpp"] || linker === compilers["c"];
    return linker === product.cpp.compilerPath;
}

function depsOutputTags() {
    return ["dep"];
}

function depsOutputArtifacts(input, product) {
    return [{
        fileTags: depsOutputTags(),
        filePath: FileInfo.joinPaths(product.destinationDirectory,
                                     input.baseName + ".dep")
    }];
}

function compilerFlags(project, product, input, outputs, explicitlyDependsOn) {
    var args = ["-c"];

    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(outputs.obj[0].fileTags));
    args = args.concat(languageFlags(tag));
    args = args.concat(targetFlags(product.qbs.targetPlatform, product.qbs.architecture,
                                   product.cpp.extenderName, product.consoleApplication,
                                   product.type));

    // Input.
    args.push(FileInfo.toWindowsSeparators(input.filePath));
    // Output.
    args.push("-o" + FileInfo.toWindowsSeparators(outputs.obj[0].filePath));
    // Preinclude headers.
    args = args.concat(Cpp.collectPreincludePaths(input).map(function(path) {
        return input.cpp.preincludeFlag + FileInfo.toWindowsSeparators(path);
    }));

    // Defines.
    args = args.concat(Cpp.collectDefinesArguments(input));

    if (tag === "cpp") {
        // We need to add the paths to the STL includes, because the DMC compiler does
        // not handle it by default (because the STL libraries is a separate port).
        var compilerIncludePaths = input.cpp.compilerIncludePaths || [];
        args = args.concat(compilerIncludePaths.map(function(path) {
            return input.cpp.includeFlag + FileInfo.toWindowsSeparators(path);
        }));
    }

    // Other includes.
    args = args.concat(Cpp.collectIncludePaths(input).map(function(path) {
        return input.cpp.includeFlag + FileInfo.toWindowsSeparators(path);
    }));
    args = args.concat(Cpp.collectSystemIncludePaths(input).map(function(path) {
        return input.cpp.systemIncludeFlag + FileInfo.toWindowsSeparators(path);
    }));

    // Debug information flags.
    if (input.cpp.debugInformation)
        args.push("-d");

    // Optimization flags.
    switch (input.cpp.optimization) {
    case "small":
        args.push("-o+space");
        break;
    case "fast":
        args.push("-o+speed");
        break;
    case "none":
        args.push("-o+none");
        break;
    }

    // Warning level flags.
    switch (input.cpp.warningLevel) {
    case "none":
        args.push("-w");
        break;
    case "all":
        args.push("-w-");
        break;
    }
    if (input.cpp.treatWarningsAsErrors)
        args.push("-wx");

    if (tag === "cpp") {
        // Exceptions flag.
        if (input.cpp.enableExceptions)
            args.push("-Ae");

        // RTTI flag.
        var enableRtti = input.cpp.enableRtti;
        if (input.cpp.enableRtti)
            args.push("-Ar");
    }

    // Listing files generation flag.
    if (input.cpp.generateCompilerListingFiles) {
        // We need to use the relative path here, because the DMC compiler does not handle
        // a long file path for this option.
        var listingPath = Cpp.relativePath(product.buildDirectory, outputs.lst[0].filePath);
        args.push("-l" + FileInfo.toWindowsSeparators(listingPath));
    }

    // Misc flags.
    args = args.concat(Cpp.collectMiscCompilerArguments(input, tag),
                       Cpp.collectMiscDriverArguments(product));
    return args;
}

function assemblerFlags(project, product, input, outputs, explicitlyDependsOn) {
    var args = ["-c"];

    // Input.
    args.push(FileInfo.toWindowsSeparators(input.filePath));
    // Output.
    args.push("-o" + FileInfo.toWindowsSeparators(outputs.obj[0].filePath));
    // Preinclude headers.
    args = args.concat(Cpp.collectPreincludePaths(input).map(function(path) {
        return input.cpp.preincludeFlag + FileInfo.toWindowsSeparators(path);
    }));

    // Defines.
    args = args.concat(Cpp.collectDefinesArguments(input));

    // Other includes.
    args = args.concat(Cpp.collectIncludePaths(input).map(function(path) {
        return input.cpp.includeFlag + FileInfo.toWindowsSeparators(path);
    }));
    args = args.concat(Cpp.collectSystemIncludePaths(input).map(function(path) {
        return input.cpp.systemIncludeFlag + FileInfo.toWindowsSeparators(path);
    }));

    // Misc flags.
    args = args.concat(Cpp.collectMiscAssemblerArguments(input, "asm"));
    return args;
}

function linkerFlags(project, product, inputs, outputs) {
    var args = [];

    var useCompilerDriver = useCompilerDriverLinker(product);
    if (useCompilerDriver) {
        args = args.concat(targetFlags(product.qbs.targetPlatform, product.qbs.architecture,
                                       product.cpp.extenderName, product.consoleApplication,
                                       product.type));

        // Input objects.
        args = args.concat(Cpp.collectLinkerObjectPaths(inputs).map(function(path) {
            return FileInfo.toWindowsSeparators(path);
        }));

        // Input resources.
        args = args.concat(Cpp.collectResourceObjectPaths(inputs).map(function(path) {
            return FileInfo.toWindowsSeparators(path);
        }));

        // Input libraries.
        args = args.concat(Cpp.collectAbsoluteLibraryDependencyPaths(product).map(function(path) {
            return FileInfo.toWindowsSeparators(path);
        }));

        // Output.
        if (product.type.includes("application")) {
            args.push("-o" + FileInfo.toWindowsSeparators(outputs.application[0].filePath));
            args.push("-L/" + (product.cpp.generateLinkerMapFile ? "MAP" : "NOMAP"));
            if (product.qbs.targetPlatform === "windows" && product.qbs.architecture === "x86")
                args.push("-L/SUBSYSTEM:" + (product.consoleApplication ? "CONSOLE" : "WINDOWS"));
        } else if (product.type.includes("dynamiclibrary")) {
            args.push("-o" + FileInfo.toWindowsSeparators(outputs.dynamiclibrary[0].filePath));
            if (product.qbs.targetPlatform === "windows" && product.qbs.architecture === "x86") {
                args.push("kernel32.lib");
                args.push("-L/IMPLIB:" + FileInfo.toWindowsSeparators(
                              outputs.dynamiclibrary_import[0].filePath));
            }
        }

        if (product.cpp.debugInformation)
            args.push("-L/DEBUG");

        args.push("-L/NOLOGO", "-L/SILENT");
    }

    // Misc flags.
    args = args.concat(Cpp.collectMiscEscapableLinkerArguments(product),
                       Cpp.collectMiscLinkerArguments(product),
                       Cpp.collectMiscDriverArguments(product));
    return args;
}

function archiverFlags(project, product, inputs, outputs) {
    var args = ["-c"];
    // Output.
    args.push(FileInfo.toWindowsSeparators(outputs.staticlibrary[0].filePath));
    // Input objects.
    args = args.concat(Cpp.collectLinkerObjectPaths(inputs).map(function(path) {
        return FileInfo.toWindowsSeparators(path);
    }));
    return args;
}

function rccCompilerFlags(project, product, input, outputs) {
    // Input.
    var args = [FileInfo.toWindowsSeparators(input.filePath)];
    // Output.
    args.push("-o" + FileInfo.toWindowsSeparators(outputs.res[0].filePath));
    // Bitness.
    args.push("-32");

    // Defines
    args = args.concat(Cpp.collectDefinesArguments(input));

    // Other includes.
    args = args.concat(Cpp.collectIncludePaths(input).map(function(path) {
        return input.cpp.includeFlag + FileInfo.toWindowsSeparators(path);
    }));
    args = args.concat(Cpp.collectSystemIncludePaths(input).map(function(path) {
        return input.cpp.systemIncludeFlag + FileInfo.toWindowsSeparators(path);
    }));
    return args;
}

function buildLinkerMapFilePath(target, suffix) {
    return FileInfo.joinPaths(FileInfo.path(target.filePath),
                              FileInfo.completeBaseName(target.fileName) + suffix);
}

// It is a workaround which removes the generated linker map file if it is disabled
// by cpp.generateLinkerMapFile property. Reason is that the DMC compiler always
// generates this file, and does not have an option to disable generation for a linker
// map file. So, we can to remove a listing files only after the linking completes.
function removeLinkerMapFile(project, product, inputs, outputs, input, output) {
    var target = outputs.dynamiclibrary ? outputs.dynamiclibrary[0]
                                        : outputs.application[0];
    var cmd = new JavaScriptCommand();
    cmd.mapFilePath = buildLinkerMapFilePath(target, ".map")
    cmd.silent = true;
    cmd.sourceCode = function() { File.remove(mapFilePath); };
    return cmd;
}

// It is a workaround to rename the extension of the output linker map file to the
// specified one, since the linker generates only files with the '.map' extension.
function renameLinkerMapFile(project, product, inputs, outputs, input, output) {
    var target = outputs.dynamiclibrary ? outputs.dynamiclibrary[0]
                                        : outputs.application[0];
    var cmd = new JavaScriptCommand();
    cmd.newMapFilePath = buildLinkerMapFilePath(target, product.cpp.linkerMapSuffix);
    cmd.oldMapFilePath = buildLinkerMapFilePath(target, ".map");
    cmd.silent = true;
    cmd.sourceCode = function() {
        if (oldMapFilePath !== newMapFilePath)
            File.move(oldMapFilePath, newMapFilePath);
    };
    return cmd;
}

function prepareCompiler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var args = compilerFlags(project, product, input, outputs, explicitlyDependsOn);
    var cmd = new Command(input.cpp.compilerPath, args);
    cmd.workingDirectory = product.buildDirectory;
    cmd.description = "compiling " + input.fileName;
    cmd.highlight = "compiler";
    cmd.jobPool = "compiler";
    return [cmd];
}

function prepareAssembler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var args = assemblerFlags(project, product, input, outputs, explicitlyDependsOn);
    var cmd = new Command(input.cpp.assemblerPath, args);
    cmd.workingDirectory = product.buildDirectory;
    cmd.description = "compiling " + input.fileName;
    cmd.highlight = "compiler";
    cmd.jobPool = "assembler";
    return [cmd];
}

function prepareLinker(project, product, inputs, outputs, input, output) {
    var cmds = [];
    var primaryOutput = outputs.dynamiclibrary ? outputs.dynamiclibrary[0]
                                               : outputs.application[0];
    var args = linkerFlags(project, product, inputs, outputs);
    var linkerPath = effectiveLinkerPath(product);
    var cmd = new Command(linkerPath, args);
    cmd.workingDirectory = product.buildDirectory;
    cmd.description = "linking " + primaryOutput.fileName;
    cmd.highlight = "linker";
    cmd.jobPool = "linker";
    cmds.push(cmd);

    if (product.cpp.generateLinkerMapFile)
        cmds.push(renameLinkerMapFile(project, product, inputs, outputs, input, output));
    else
        cmds.push(removeLinkerMapFile(project, product, inputs, outputs, input, output));

    return cmds;
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

function prepareRccCompiler(project, product, inputs, outputs, input, output) {
    var args = rccCompilerFlags(project, product, input, outputs);
    var cmd = new Command(input.cpp.rccCompilerPath, args);
    cmd.workingDirectory = product.buildDirectory;
    cmd.description = "compiling " + input.fileName;
    cmd.highlight = "compiler";
    cmd.jobPool = "compiler";
    return [cmd];
}
