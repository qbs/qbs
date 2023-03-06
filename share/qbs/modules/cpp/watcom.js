/****************************************************************************
**
** Copyright (C) 2022 Denis Shienkov <denis.shienkov@gmail.com>
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

function toolchainDetails(qbs) {
    var platform = qbs.targetPlatform;
    var details = {};
    if (platform === "dos") {
        details.imageFormat = "mz";
        details.executableSuffix = ".exe";
    } else if (platform === "os2") {
        details.imageFormat = "pe";
        details.executableSuffix = ".exe";
        details.dynamicLibrarySuffix = ".dll";
    } else if (platform === "windows") {
        details.imageFormat = "pe";
        details.executableSuffix = ".exe";
        details.dynamicLibrarySuffix = ".dll";
    } else if (platform === "linux") {
        details.imageFormat = "elf";
        details.executableSuffix = "";
        details.dynamicLibrarySuffix = ".so";
    }
    return details;
}

function languageFlag(tag) {
    if (tag === "c")
        return "-xc";
    else if (tag === "cpp")
        return "-xc++";
}

function targetFlag(platform, architecture, type) {
    if (platform === "dos") {
        if (architecture === "x86_16")
            return "-bdos";
        else if (architecture === "x86")
            return "-bdos4g";
    } else if (platform === "os2") {
        if (architecture === "x86_16")
            return "-bos2";
        else if (architecture === "x86")
            return "-bos2v2";
    } else if (platform === "windows") {
        if (architecture === "x86_16") {
            if (type.includes("dynamiclibrary"))
                return "-bwindows_dll";
            return "-bwindows";
        } else if (architecture === "x86") {
            if (type.includes("dynamiclibrary"))
                return "-bnt_dll";
            return "-bnt";
        }
    } else if (platform === "linux") {
        return "-blinux";
    }
}

function guessVersion(macros) {
    var version = parseInt(macros["__WATCOMC__"], 10)
            || parseInt(macros["__WATCOM_CPLUSPLUS__"], 10);
    if (version) {
        return { major: parseInt((version - 1100) / 100),
            minor: parseInt(version / 10) % 10,
            patch: ((version % 10) > 0) ? parseInt(version % 10) : 0 }
    }
}

function guessEnvironment(hostOs, platform, architecture,
                          toolchainInstallPath, pathListSeparator) {
    var toolchainRootPath = FileInfo.path(toolchainInstallPath);
    if (!File.exists(toolchainRootPath)) {
        throw "Unable to deduce environment due to compiler root directory: '"
                + toolchainRootPath + "' does not exist";
    }

    var env = {};

    function setVariable(key, properties, path, separator) {
        var values = [];
        for (var i = 0; i < properties.length; ++i) {
            if (path) {
                var fullpath = FileInfo.joinPaths(path, properties[i]);
                values.push(FileInfo.toNativeSeparators(fullpath));
            } else {
                values.push(properties[i]);
            }
        }
        env[key] = values.join(separator);
    }

    setVariable("WATCOM", [toolchainRootPath], undefined, pathListSeparator);
    setVariable("EDPATH", ["eddat"], toolchainRootPath, pathListSeparator);

    if (hostOs.includes("linux"))
        setVariable("PATH", ["binl64", "binl"], toolchainRootPath, pathListSeparator);
    else if (hostOs.includes("windows"))
        setVariable("PATH", ["binnt64", "binnt"], toolchainRootPath, pathListSeparator);

    if (platform === "linux") {
        setVariable("INCLUDE", ["lh"], toolchainRootPath, pathListSeparator);
    } else {
        // Common for DOS, Windows, OS/2.
        setVariable("WIPFC", ["wipfc"], toolchainRootPath, pathListSeparator);
        setVariable("WHTMLHELP", ["binnt/help"], toolchainRootPath, pathListSeparator);

        var includes = ["h"];
        if (platform === "dos") {
            // Same includes as before.
        } else if (platform === "os2") {
            if (architecture === "x86")
                includes = includes.concat(["h/os2"]);
            else if (architecture === "x86_16")
                includes = includes.concat(["h/os21x"]);
        } else if (platform === "windows") {
            if (architecture === "x86")
                includes = includes.concat(["h/nt", "h/nt/directx", "h/nt/ddk"]);
            else if (architecture === "x86_16")
                includes = includes.concat(["h/win"]);
        } else {
            throw "Unable to deduce environment for unsupported target platform: '"
                    + platform + "'";
        }

        setVariable("INCLUDE", includes, toolchainRootPath, pathListSeparator);
    }

    return env;
}

function dumpMacros(environment, compilerPath, platform, architecture, tag) {
    // Note: The Open Watcom compiler does not support the predefined
    // macros dumping. So, we do it with the following trick, where we try
    // to create and compile a special temporary file and to parse the console
    // output with the own magic pattern: #define <key> <value>.

    var outputDirectory = new TemporaryDir();
    var outputFilePath = FileInfo.joinPaths(outputDirectory.path(), "dump-macros.c");
    var outputFile = new TextFile(outputFilePath, TextFile.WriteOnly);
    outputFile.writeLine("#define VALUE_TO_STRING(x) #x");
    outputFile.writeLine("#define VALUE(x) VALUE_TO_STRING(x)");
    outputFile.writeLine("#define VAR_NAME_VALUE(var) \"#define \"#var\" \"VALUE(var)");
    // Declare all available pre-defined macros of Watcon compiler.
    var keys = [
                // Prepare the DOS target macros.
                "__DOS__", "_DOS", "MSDOS",
                // Prepare the OS/2 target macros.
                "__OS2__",
                // Prepare the QNX target macros.
                "__QNX__",
                // Prepare the Netware target macros.
                "__NETWARE__", "__NETWARE_386__",
                // Prepare the Windows target macros.
                "__NT__", "__WINDOWS__", "_WINDOWS", "__WINDOWS_386__",
                // Prepare the Linux and Unix target macros.
                "__LINUX__", "__UNIX__",
                // Prepare the 16-bit target specific macros.
                "__I86__", "M_I86", "_M_I86", "_M_IX86",
                // Prepare the 32-bit target specific macros.
                "__386__", "M_I386", "_M_I386", "_M_IX86",
                // Prepare the indicated options macros.
                "_MT", "_DLL", "__FPI__", "__CHAR_SIGNED__", "__INLINE_FUNCTIONS__",
                "_CPPRTTI", "_CPPUNWIND", "NO_EXT_KEYS",
                // Prepare the common memory model macros.
                "__FLAT__", "__SMALL__", "__MEDIUM__",
                "__COMPACT__", "__LARGE__", "__HUGE__",
                // Prepare the 16-bit memory model macros.
                "M_I86SM", "_M_I86SM", "M_I86MM", "_M_I86MM", "M_I86CM",
                "_M_I86CM", "M_I86LM", "_M_I86LM", "M_I86HM", "_M_I86HM",
                // Prepare the 32-bit memory model macros.
                "M_386FM", "_M_386FM", "M_386SM", "M_386MM", "_M_386MM",
                "M_386CM", "_M_386CM", "M_386LM", "_M_386LM",
                // Prepare the compiler macros.
                "__X86__", "__cplusplus", "__WATCOMC__", "__WATCOM_CPLUSPLUS__",
                "_INTEGRAL_MAX_BITS", "_PUSHPOP_SUPPORTED", "_STDCALL_SUPPORTED",
                // Prepare the other macros.
                "__3R__", "_based", "_cdecl", "cdecl", "_export", "_far16", "_far", "far",
                "_fastcall", "_fortran", "fortran", "_huge", "huge", "_inline", "_interrupt",
                "interrupt", "_loadds", "_near", "near", "_pascal", "pascal", "_saveregs",
                "_segment", "_segname", "_self", "SOMDLINK", "_STDCALL_SUPPORTED", "__SW_0",
                "__SW_3R", "__SW_5", "__SW_FP287", "__SW_FP2", "__SW_FP387", "__SW_FP3",
                "__SW_FPI", "__SW_MF", "__SW_MS", "__SW_ZDP", "__SW_ZFP", "__SW_ZGF",
                "__SW_ZGP", "_stdcall", "_syscall", "__BIG_ENDIAN"
            ];
    for (var i = 0; i < keys.length; ++i) {
        var key = keys[i];
        outputFile.writeLine("#if defined(" + key + ")");
        outputFile.writeLine("#pragma message (VAR_NAME_VALUE(" + key + "))");
        outputFile.writeLine("#endif");
    }
    outputFile.close();

    var process = new Process();
    process.setWorkingDirectory(outputDirectory.path());
    for (var envkey in environment)
        process.setEnv(envkey, environment[envkey]);

    var target = targetFlag(platform, architecture, ["application"]);
    var lang = languageFlag(tag);
    var args = [ target, lang, outputFilePath ];

    process.exec(compilerPath, args, false);
    var m = Cpp.extractMacros(process.readStdOut(), /"?(#define(\s\w+){1,2})"?$/);
    if (tag === "cpp" && m["__cplusplus"] === "1")
        return m;
    else if (tag === "c")
        return m;
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
    var compiler = product.cpp.compilerPath;
    return linker === compiler;
}

function escapeLinkerFlags(useCompilerDriver, linkerFlags) {
    if (!linkerFlags || linkerFlags.length === 0)
        return [];

    if (useCompilerDriver) {
        var sep = ",";
        return [["-Wl"].concat(linkerFlags).join(sep)];
    }
    return linkerFlags;
}

function assemblerFlags(project, product, input, outputs, explicitlyDependsOn) {
    var args = [FileInfo.toNativeSeparators(input.filePath)];
    args.push("-fo=" + FileInfo.toNativeSeparators(outputs.obj[0].filePath));

    args = args.concat(Cpp.collectPreincludePaths(input).map(function(path) {
        return "-fi" + FileInfo.toNativeSeparators(path);
    }));

    args = args.concat(Cpp.collectDefinesArguments(input, "-d"));

    var includePaths = [].concat(Cpp.collectIncludePaths(input)).concat(
                Cpp.collectSystemIncludePaths(input));
    args = args.concat(includePaths.map(function(path) {
        return "-i" + FileInfo.toNativeSeparators(path);
    }));

    if (input.cpp.debugInformation)
        args.push("-d1");

    var warnings = input.cpp.warningLevel
    if (warnings === "none")
        args.push("-w0");
    else if (warnings === "all")
        args.push("-wx");
    if (input.cpp.treatWarningsAsErrors)
        args.push("-we");

    args.push("-zq"); // Silent.
    args = args.concat(Cpp.collectMiscAssemblerArguments(input));
    return args;
}

function compilerFlags(project, product, input, outputs, explicitlyDependsOn) {
    var args = ["-g" + (input.cpp.debugInformation ? "3" : "0")];

    var target = targetFlag(product.qbs.targetPlatform, product.qbs.architecture,
                            product.type);
    args.push(target);

    if (product.type.includes("application")) {
        if (product.qbs.targetPlatform === "windows") {
            var consoleApplication = product.consoleApplication;
            args.push(consoleApplication ? "-mconsole" : "-mwindows");
        }
    } else if (product.type.includes("dynamiclibrary")) {
        args.push("-shared");
    }

    var optimization = input.cpp.optimization
    if (optimization === "fast")
        args.push("-Ot");
    else if (optimization === "small")
        args.push("-Os");
    else if (optimization === "none")
        args.push("-O0");

    var warnings = input.cpp.warningLevel
    if (warnings === "none") {
        args.push("-w");
    } else if (warnings === "all") {
        args.push("-Wall");
        args.push("-Wextra");
    }
    if (input.cpp.treatWarningsAsErrors)
        args.push("-Werror");

    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(outputs.obj[0].fileTags));

    var langFlag = languageFlag(tag);
    if (langFlag)
        args.push(langFlag);

    if (tag === "cpp") {
        var enableExceptions = input.cpp.enableExceptions;
        if (enableExceptions) {
            var ehModel = input.cpp.exceptionHandlingModel;
            switch (ehModel) {
            case "direct":
                args.push("-feh-direct");
                break;
            case "table":
                args.push("-feh-table");
                break;
            default:
                args.push("-feh");
                break;
            }
        } else {
            args.push("-fno-eh");
        }

        var enableRtti = input.cpp.enableRtti;
        args.push(enableRtti ? "-frtti" : "-fno-rtti");
    } else if (tag === "c") {
        var knownValues = ["c99", "c89"];
        var cLanguageVersion = Cpp.languageVersion(input.cpp.cLanguageVersion, knownValues, "C");
        switch (cLanguageVersion) {
        case "c89":
            args.push("-std=c89");
            break;
        case "c99":
            args.push("-std=c99");
            break;
        }
    }

    var preincludePaths = Cpp.collectPreincludePaths(input);
    for (var i = 0; i < preincludePaths.length; ++i)
        args.push(input.cpp.preincludeFlag, preincludePaths[i]);

    args = args.concat(Cpp.collectDefinesArguments(input));

    args = args.concat(Cpp.collectIncludePaths(input).map(function(path) {
        return input.cpp.includeFlag + FileInfo.toNativeSeparators(path);
    }));
    args = args.concat(Cpp.collectSystemIncludePaths(input).map(function(path) {
        return input.cpp.systemIncludeFlag + FileInfo.toNativeSeparators(path);
    }));

    args = args.concat(Cpp.collectMiscCompilerArguments(input, tag),
                       Cpp.collectMiscDriverArguments(input));

    args.push("-o", FileInfo.toNativeSeparators(outputs.obj[0].filePath));
    args.push("-c", FileInfo.toNativeSeparators(input.filePath));

    return args;
}

function resourceCompilerFlags(project, product, input, outputs) {
    var args = [input.filePath];
    args.push("-fo=" + FileInfo.toNativeSeparators(outputs.res[0].filePath));
    args = args.concat(Cpp.collectDefinesArguments(input, "-d"));

    args = args.concat(Cpp.collectIncludePaths(input).map(function(path) {
        return input.cpp.includeFlag + FileInfo.toNativeSeparators(path);
    }));
    args = args.concat(Cpp.collectSystemIncludePaths(input).map(function(path) {
        return input.cpp.includeFlag + FileInfo.toNativeSeparators(path);
    }));
    args.push("-q", "-ad", "-r");
    return args;
}

function linkerFlags(project, product, inputs, outputs) {
    var args = [];
    var useCompilerDriver = useCompilerDriverLinker(product);
    if (useCompilerDriver) {
        var target = targetFlag(product.qbs.targetPlatform, product.qbs.architecture,
                                product.type);
        args.push(target);

        if (product.type.includes("application")) {
            args.push("-o", FileInfo.toNativeSeparators(outputs.application[0].filePath));
            if (product.cpp.generateLinkerMapFile)
                args.push("-fm=" + FileInfo.toNativeSeparators(outputs.mem_map[0].filePath));
        } else if (product.type.includes("dynamiclibrary")) {
            if (product.qbs.targetPlatform === "windows") {
                args.push("-Wl, option implib=" + FileInfo.toNativeSeparators(
                              outputs.dynamiclibrary_import[0].filePath));
            }
            args.push("-o", FileInfo.toNativeSeparators(outputs.dynamiclibrary[0].filePath))
        }

        var escapableLinkerFlags = [];
        var targetLinkerFlags = product.cpp.targetLinkerFlags;
        if (targetLinkerFlags)
            escapableLinkerFlags = escapableLinkerFlags.concat(targetLinkerFlags);

        escapableLinkerFlags = escapableLinkerFlags.concat(
                    Cpp.collectMiscEscapableLinkerArguments(product));

        var escapedLinkerFlags = escapeLinkerFlags(useCompilerDriver, escapableLinkerFlags);
        if (escapedLinkerFlags)
            args = args.concat(escapedLinkerFlags);

        args = args.concat(Cpp.collectLibraryPaths(product).map(function(path) {
            return product.cpp.libraryPathFlag + FileInfo.toNativeSeparators(path);
        }));
        args = args.concat(Cpp.collectLinkerObjectPaths(inputs).map(function(path) {
            return FileInfo.toNativeSeparators(path);
        }));

        var libraryDependencies = Cpp.collectLibraryDependencies(product);
        for (var i = 0; i < libraryDependencies.length; ++i) {
            var lib = libraryDependencies[i].filePath;
            if (FileInfo.isAbsolutePath(lib) || lib.startsWith('@'))
                args.push(FileInfo.toNativeSeparators(lib));
            else
                args.push("-Wl, libfile " + lib);
        }

        var resourcePaths = Cpp.collectResourceObjectPaths(inputs).map(function(path) {
            return FileInfo.toNativeSeparators(path);
        });
        if (resourcePaths.length > 0)
            args = args.concat("-Wl, resource " + resourcePaths.join(","));
    }

    args = args.concat(Cpp.collectMiscLinkerArguments(product),
                       Cpp.collectMiscDriverArguments(product));
    return args;
}

function libraryManagerFlags(project, product, inputs, outputs) {
    var args = ["-b", "-n", "-q"];
    args = args.concat(Cpp.collectLinkerObjectPaths(inputs).map(function(path) {
        return "+" + FileInfo.toNativeSeparators(path);
    }));
    args.push("-o", FileInfo.toNativeSeparators(outputs.staticlibrary[0].filePath));
    return args;
}

function disassemblerFlags(project, product, inputs, outputs) {
    var objectPath = Cpp.relativePath(product.buildDirectory, outputs.obj[0].filePath);
    var listingPath = Cpp.relativePath(product.buildDirectory, outputs.lst[0].filePath);
    var args = [];
    args.push(FileInfo.toNativeSeparators(objectPath));
    args.push("-l=" + FileInfo.toNativeSeparators(listingPath));
    args.push("-s", "-a");
    return args;
}

function generateCompilerListing(project, product, inputs, outputs, input, output) {
    var args = disassemblerFlags(project, product, input, outputs);
    var cmd = new Command(input.cpp.disassemblerPath, args);
    cmd.workingDirectory = product.buildDirectory;
    cmd.silent = true;
    cmd.jobPool = "watcom_job_pool";
    return cmd;
}

function prepareAssembler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var cmds = [];
    var args = assemblerFlags(project, product, input, outputs);
    var cmd = new Command(input.cpp.assemblerPath, args);
    cmd.workingDirectory = product.buildDirectory;
    cmd.description = "assembling " + input.fileName;
    cmd.highlight = "compiler";
    cmd.jobPool = "watcom_job_pool";
    cmds.push(cmd);
    if (input.cpp.generateAssemblerListingFiles)
        cmds.push(generateCompilerListing(project, product, inputs, outputs, input, output));
    return cmds;
}

function prepareCompiler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var cmds = [];
    var args = compilerFlags(project, product, input, outputs, explicitlyDependsOn);
    var cmd = new Command(input.cpp.compilerPath, args);
    cmd.workingDirectory = product.buildDirectory;
    cmd.description = "compiling " + input.fileName;
    cmd.highlight = "compiler";
    cmd.jobPool = "watcom_job_pool";
    cmds.push(cmd);
    if (input.cpp.generateCompilerListingFiles)
        cmds.push(generateCompilerListing(project, product, inputs, outputs, input, output));
    return cmds;
}

function prepareResourceCompiler(project, product, inputs, outputs, input, output,
                                 explicitlyDependsOn) {
    var args = resourceCompilerFlags(project, product, input, outputs);
    var cmd = new Command(input.cpp.resourceCompilerPath, args);
    // Set working directory to source directory as a workaround
    // to make the resources compilable by resource compiler (it is magic).
    cmd.workingDirectory = product.sourceDirectory;
    cmd.description = "compiling " + input.fileName;
    cmd.highlight = "compiler";
    cmd.jobPool = "watcom_job_pool";
    return [cmd];
}

function prepareLinker(project, product, inputs, outputs, input, output) {
    var primaryOutput = outputs.dynamiclibrary ? outputs.dynamiclibrary[0]
                                               : outputs.application[0];
    var args = linkerFlags(project, product, inputs, outputs);
    var linkerPath = effectiveLinkerPath(product);
    var cmd = new Command(linkerPath, args);
    cmd.workingDirectory = product.buildDirectory;
    cmd.description = "linking " + primaryOutput.fileName;
    cmd.highlight = "linker";
    cmd.jobPool = "watcom_job_pool";
    return [cmd];
}

function prepareLibraryManager(project, product, inputs, outputs, input, output) {
    var args = libraryManagerFlags(project, product, inputs, outputs);
    var cmd = new Command(product.cpp.libraryManagerPath, args);
    cmd.workingDirectory = product.buildDirectory;
    cmd.description = "linking " + outputs.staticlibrary[0].fileName;
    cmd.highlight = "linker";
    cmd.jobPool = "watcom_job_pool";
    return [cmd];
}
