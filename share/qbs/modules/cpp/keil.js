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

function isMcs51Architecture(architecture) {
    return architecture === "mcs51";
}

function isMcs251Architecture(architecture) {
    return architecture === "mcs251";
}

function isMcsArchitecture(architecture) {
    return isMcs51Architecture(architecture)
        || isMcs251Architecture(architecture);
}

function isC166Architecture(architecture) {
    return architecture === "c166";
}

function isArmArchitecture(architecture) {
    return architecture.startsWith("arm");
}

function isMcsCompiler(compilerPath) {
    var base = FileInfo.baseName(compilerPath).toLowerCase();
    return base === "c51" || base === "cx51" || base === "c251";
}

function isC166Compiler(compilerPath) {
    return FileInfo.baseName(compilerPath).toLowerCase() === "c166";
}

function isArmCCCompiler(compilerPath) {
    return FileInfo.baseName(compilerPath).toLowerCase() === "armcc";
}

function isArmClangCompiler(compilerPath) {
    return FileInfo.baseName(compilerPath).toLowerCase() === "armclang";
}

function preincludeFlag(compilerPath) {
    if (isArmCCCompiler(compilerPath))
        return "--preinclude";
    else if (isArmClangCompiler(compilerPath))
        return "-include";
}

function toolchainDetails(qbs) {
    var architecture = qbs.architecture;
    if (isMcs51Architecture(architecture)) {
        return {
            "imageFormat": "omf",
            "linkerMapSuffix":".m51",
            "objectSuffix": ".obj",
            "executableSuffix": ".abs",
            "compilerName": "c51",
            "assemblerName": "a51",
            "linkerName": "bl51",
            "archiverName": "lib51"
        };
    } else if (isMcs251Architecture(architecture)) {
        return {
            "imageFormat": "omf",
            "linkerMapSuffix":".m51",
            "objectSuffix": ".obj",
            "executableSuffix": ".abs",
            "compilerName": "c251",
            "assemblerName": "a251",
            "linkerName": "l251",
            "archiverName": "lib251"
        };
    } else if (isC166Architecture(architecture)) {
        return {
            "imageFormat": "omf",
            "linkerMapSuffix":".m66",
            "objectSuffix": ".obj",
            "executableSuffix": ".abs",
            "compilerName": "c166",
            "assemblerName": "a166",
            "linkerName": "l166",
            "archiverName": "lib166"
        };
    } else if (isArmArchitecture(architecture)) {
        return {
            "imageFormat": "elf",
            "linkerMapSuffix":".map",
            "objectSuffix": ".o",
            "executableSuffix": ".axf",
            "disassemblerName": "fromelf",
            "compilerName": "armcc",
            "assemblerName": "armasm",
            "linkerName": "armlink",
            "archiverName": "armar"
        };
    }
}

function guessArmCCArchitecture(targetArchArm, targetArchThumb) {
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

function guessArmClangArchitecture(targetArchArm, targetArchProfile) {
    targetArchProfile = targetArchProfile.replace(/'/g, "");
    var arch = "arm";
    if (targetArchArm !== "" && targetArchProfile !== "")
        arch += "v" + targetArchArm + targetArchProfile.toLowerCase();
    return arch;
}

function guessArchitecture(macros) {
    if (macros["__C51__"])
        return "mcs51";
    else if (macros["__C251__"])
        return "mcs251";
    else if (macros["__C166__"])
        return "c166";
    else if (macros["__CC_ARM"] === "1")
        return guessArmCCArchitecture(macros["__TARGET_ARCH_ARM"], macros["__TARGET_ARCH_THUMB"]);
    else if (macros["__clang__"] === "1" && macros["__arm__"] === "1")
        return guessArmClangArchitecture(macros["__ARM_ARCH"], macros["__ARM_ARCH_PROFILE"]);
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
    } else if (macros["__C166__"]) {
        // The C166 processors are 16-bit. So, the data with an integer type
        // represented by more than one byte is stored as little endian in the
        // Keil toolchain. See for more info:
        // * http://www.keil.com/support/man/docs/c166/c166_ap_ints.htm
        // * http://www.keil.com/support/man/docs/c166/c166_ap_longs.htm
        return "little";
    } else if (macros["__CC_ARM"]) {
        return macros["__BIG_ENDIAN"] ? "big" : "little";
    } else if (macros["__clang__"] && macros["__arm__"]) {
        switch (macros["__BYTE_ORDER__"]) {
        case "__ORDER_BIG_ENDIAN__":
            return "big";
        case "__ORDER_LITTLE_ENDIAN__":
            return "little";
        }
    }
}

function guessVersion(macros) {
    if (macros["__C51__"] || macros["__C251__"]) {
        var mcsVersion = macros["__C51__"] || macros["__C251__"];
        return { major: parseInt(mcsVersion / 100),
            minor: parseInt(mcsVersion % 100),
            patch: 0 }
    } else if (macros["__C166__"]) {
        var xcVersion = macros["__C166__"];
        return { major: parseInt(xcVersion / 100),
            minor: parseInt(xcVersion % 100),
            patch: 0 }
    } else if (macros["__CC_ARM"] || macros["__clang__"]) {
        var armVersion = macros["__ARMCC_VERSION"];
        return { major: parseInt(armVersion / 1000000),
            minor: parseInt(armVersion / 10000) % 100,
            patch: parseInt(armVersion) % 10000 }
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

    var outputDirectory = new TemporaryDir();
    var outputFilePath = FileInfo.fromNativeSeparators(FileInfo.joinPaths(outputDirectory.path(),
                                                       "dump-macros.c"));
    var outputFile = new TextFile(outputFilePath, TextFile.WriteOnly);

    outputFile.writeLine("#define VALUE_TO_STRING(x) #x");
    outputFile.writeLine("#define VALUE(x) VALUE_TO_STRING(x)");

    // Predefined keys for C51 and C251 compilers, see details:
    // * https://www.keil.com/support/man/docs/c51/c51_pp_predefmacroconst.htm
    // * https://www.keil.com/support/man/docs/c251/c251_pp_predefmacroconst.htm
    var keys = ["__C51__", "__CX51__", "__C251__", "__MODEL__",
                "__STDC__", "__FLOAT64__", "__MODSRC__"];

    // For C51 compiler.
    outputFile.writeLine("#if defined(__C51__) || defined(__CX51__)");
    outputFile.writeLine("#  define VAR_NAME_VALUE(var) \"(\"\"\"\"|\"#var\"|\"VALUE(var)\"|\"\"\"\")\"");
    for (var i in keys) {
        var key = keys[i];
        outputFile.writeLine("#  if defined (" + key + ")");
        outputFile.writeLine("#    pragma message (VAR_NAME_VALUE(" + key + "))");
        outputFile.writeLine("#  endif");
    }
    outputFile.writeLine("#endif");

    // For C251 compiler.
    outputFile.writeLine("#if defined(__C251__)");
    outputFile.writeLine("#  define VAR_NAME_VALUE(var) \"\"|#var|VALUE(var)|\"\"");
    for (var i in keys) {
        var key = keys[i];
        outputFile.writeLine("#  if defined (" + key + ")");
        outputFile.writeLine("#    warning (VAR_NAME_VALUE(" + key + "))");
        outputFile.writeLine("#  endif");
    }
    outputFile.writeLine("#endif");

    outputFile.close();

    var process = new Process();
    process.exec(compilerFilePath, [outputFilePath], false);
    File.remove(outputFilePath);
    var map = {};
    process.readStdOut().trim().split(/\r?\n/g).map(function(line) {
        var parts = line.split("\"|\"", 4);
        if (parts.length === 4)
            map[parts[1]] = parts[2];
    });
    return map;
}

function dumpC166CompilerMacros(compilerFilePath, tag) {
    // C166 compiler support only C language.
    if (tag === "cpp")
        return {};

    // Note: The C166 compiler does not support the predefined
    // macros dumping. Also, it does not support the '#pragma' and
    // '#message|warning|error' directives properly (it is impossible
    // to print to console the value of macro).
    // So, we do it with the following trick, where we try
    // to create and compile a special temporary file and to parse the console
    // output with the own magic pattern, e.g:
    //
    // *** WARNING C320 IN LINE 41 OF c51.c: __C166__
    // *** WARNING C2 IN LINE 42 OF c51.c: '757': unknown #pragma/control, line ignored
    //
    // where the '__C166__' is a key, and the '757' is a value.

    var outputDirectory = new TemporaryDir();
    var outputFilePath = FileInfo.fromNativeSeparators(outputDirectory.path() + "/dump-macros.c");
    var outputFile = new TextFile(outputFilePath, TextFile.WriteOnly);

    // Predefined keys for C166 compiler, see details:
    // * https://www.keil.com/support/man/docs/c166/c166_pp_predefmacroconst.htm
    var keys = ["__C166__", "__DUS__", "__MAC__", "__MOD167__",
                "__MODEL__", "__MODV2__", "__SAVEMAC__", "__STDC__"];

    // For C166 compiler.
    outputFile.writeLine("#if defined(__C166__)");
    for (var i in keys) {
        var key = keys[i];
        outputFile.writeLine("#  if defined (" + key + ")");
        outputFile.writeLine("#    warning " + key);
        outputFile.writeLine("#    pragma " + key);
        outputFile.writeLine("#  endif");
    }
    outputFile.writeLine("#endif");

    outputFile.close();

    function extractKey(line, knownKeys) {
        for (var i in keys) {
            var key = knownKeys[i];
            var regexp = new RegExp("^\\*\\*\\* WARNING C320 IN LINE .+: (" + key + ")$");
            var match = regexp.exec(line);
            if (match)
                return key;
        }
    };

    function extractValue(line) {
        var regexp = new RegExp("^\\*\\*\\* WARNING C2 IN LINE .+'(.+)':.+$");
        var match = regexp.exec(line);
        if (match)
            return match[1];
    };

    var process = new Process();
    process.exec(compilerFilePath, [outputFilePath], false);
    File.remove(outputFilePath);
    var lines = process.readStdOut().trim().split(/\r?\n/g);
    var map = {};
    for (var i = 0; i < lines.length; ++i) {
        // First line should contains the macro key.
        var keyLine = lines[i];
        var key = extractKey(keyLine, keys);
        if (!key)
            continue;

        i += 1;
        if (i >= lines.length)
            break;

        // Second line should contains the macro value.
        var valueLine = lines[i];
        var value = extractValue(valueLine);
        if (!value)
            continue;
        map[key] = value;
    }
    return map;
}

function dumpArmCCCompilerMacros(compilerFilePath, tag, nullDevice) {
    var args = [ "-E", "--list-macros", "--cpu=cortex-m0", nullDevice ];
    if (tag === "cpp")
        args.push("--cpp");

    var p = new Process();
    p.exec(compilerFilePath, args, false);
    return Cpp.extractMacros(p.readStdOut());
}

function dumpArmClangCompilerMacros(compilerFilePath, tag, nullDevice) {
    var args = [ "--target=arm-arm-none-eabi", "-mcpu=cortex-m0", "-dM", "-E",
                "-x", ((tag === "cpp") ? "c++" : "c"), nullDevice ];
    var p = new Process();
    p.exec(compilerFilePath, args, false);
    return Cpp.extractMacros(p.readStdOut());
}

function dumpMacros(compilerFilePath, tag, nullDevice) {
    if (isMcsCompiler(compilerFilePath))
        return dumpMcsCompilerMacros(compilerFilePath, tag, nullDevice);
    else if (isC166Compiler(compilerFilePath))
        return dumpC166CompilerMacros(compilerFilePath, tag, nullDevice);
    else if (isArmCCCompiler(compilerFilePath))
        return dumpArmCCCompilerMacros(compilerFilePath, tag, nullDevice);
    else if (isArmClangCompiler(compilerFilePath))
        return dumpArmClangCompilerMacros(compilerFilePath, tag, nullDevice);
    return {};
}

function dumpMcsCompilerIncludePaths(compilerFilePath) {
    var includePath = compilerFilePath.replace(/bin[\\\/](.*)$/i, "inc");
    return [includePath];
}

function dumpC166CompilerIncludePaths(compilerFilePath) {
    // Same as for MCS compiler.
    return dumpMcsCompilerIncludePaths(compilerFilePath);
}

function dumpArmCCCompilerIncludePaths(compilerFilePath) {
    var includePath = compilerFilePath.replace(/bin[\\\/](.*)$/i, "include");
    return [includePath];
}

function dumpArmClangCompilerIncludePaths(compilerFilePath, nullDevice) {
    var args = [ "--target=arm-arm-none-eabi", "-mcpu=cortex-m0", "-v", "-E",
                "-x", "c++", nullDevice ];
    var p = new Process();
    p.exec(compilerFilePath, args, false);
    var lines = p.readStdErr().trim().split(/\r?\n/g).map(function (line) { return line.trim(); });
    var addIncludes = false;
    var includePaths = [];
    for (var i = 0; i < lines.length; ++i) {
        var line = lines[i];
        if (line === "#include <...> search starts here:")
            addIncludes = true;
        else if (line === "End of search list.")
            addIncludes = false;
        else if (addIncludes)
            includePaths.push(line);
    }
    return includePaths;
}

function dumpDefaultPaths(compilerFilePath, nullDevice) {
    var includePaths = [];
    if (isMcsCompiler(compilerFilePath))
        includePaths = dumpMcsCompilerIncludePaths(compilerFilePath);
    else if (isC166Compiler(compilerFilePath))
        includePaths = dumpC166CompilerIncludePaths(compilerFilePath);
    else if (isArmCCCompiler(compilerFilePath))
        includePaths = dumpArmCCCompilerIncludePaths(compilerFilePath);
    else if (isArmClangCompiler(compilerFilePath))
        includePaths = dumpArmClangCompilerIncludePaths(compilerFilePath, nullDevice);
    return { "includePaths": includePaths };
}

function filterMcsOutput(output) {
    var filteredLines = [];
    output.split(/\r\n|\r|\n/).forEach(function(line) {
        var regexp = /^\s*\*{3}\s|^\s{29}|^\s{4}\S|^\s{2}[0-9A-F]{4}|^\s{21,25}\d+|^[0-9A-F]{4}\s\d+/;
        if (regexp.exec(line))
            filteredLines.push(line);
    });
    return filteredLines.join('\n');
};

function filterC166Output(output) {
    var filteredLines = [];
    output.split(/\r\n|\r|\n/).forEach(function(line) {
        var regexp = /^\s*\*{3}\s|^\s{29}|^\s{27,28}\d+|^\s{21}\d+|^\s{4}\S|^\s{2}[0-9A-F]{4}|^[0-9A-F]{4}\s\d+/;
        if (regexp.exec(line))
            filteredLines.push(line);
    });
    return filteredLines.join('\n');
};

function compilerFlags(project, product, input, outputs, explicitlyDependsOn) {
    // Determine which C-language we're compiling.
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(outputs.obj[0].fileTags));
    var args = [];

    var architecture = input.qbs.architecture;
    if (isMcsArchitecture(architecture) || isC166Architecture(architecture)) {
        // Input.
        args.push(input.filePath);

        // Output.
        args.push("OBJECT(" + FileInfo.toWindowsSeparators(outputs.obj[0].filePath) + ")");

        // Defines.
        var defines = Cpp.collectDefines(input);
        if (defines.length > 0)
            args = args.concat("DEFINE (" + defines.join(",") + ")");

        // Includes.
        var allIncludePaths = [].concat(Cpp.collectIncludePaths(input),
                                        Cpp.collectSystemIncludePaths(input));
        if (allIncludePaths.length > 0)
            args = args.concat("INCDIR(" + allIncludePaths.map(function(path) {
                return FileInfo.toWindowsSeparators(path); }).join(";") + ")");

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
            if (isMcs51Architecture(architecture))
                args.push("FARWARNING");
            break;
        }

        // Listing files generation flag.
        if (!input.cpp.generateCompilerListingFiles)
            args.push("NOPRINT");
        else
            args.push("PRINT(" + FileInfo.toWindowsSeparators(outputs.lst[0].filePath) + ")");
    } else if (isArmArchitecture(architecture)) {
        // Input.
        args.push("-c", input.filePath);

        // Output.
        args.push("-o", outputs.obj[0].filePath);

        // Preinclude headers.
        args = args.concat(Cpp.collectPreincludePathsArguments(input, true));

        // Defines.
        args = args.concat(Cpp.collectDefinesArguments(input));

        // Includes.
        args = args.concat(Cpp.collectIncludePathsArguments(input));
        args = args.concat(Cpp.collectSystemIncludePathsArguments(input));

        var compilerPath = input.cpp.compilerPath;
        if (isArmCCCompiler(compilerPath)) {
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
            if (input.cpp.generateCompilerListingFiles) {
                args.push("--list");
                args.push("--list_dir", FileInfo.path(outputs.lst[0].filePath));
            }
        } else if (isArmClangCompiler(compilerPath)) {
            // Debug information flags.
            if (input.cpp.debugInformation)
                args.push("-g");

            // Optimization level flags.
            switch (input.cpp.optimization) {
            case "small":
                args.push("-Oz")
                break;
            case "fast":
                args.push("-Ofast")
                break;
            case "none":
                args.push("-O0")
                break;
            }

            // Warning level flags.
            switch (input.cpp.warningLevel) {
            case "all":
                args.push("-Wall");
                break;
            default:
                break;
            }

            if (input.cpp.treatWarningsAsErrors)
                args.push("-Werror");

            if (tag === "c") {
                // C language version flags.
                var knownCLanguageValues = ["c11", "c99", "c90", "c89"];
                var cLanguageVersion = Cpp.languageVersion(
                            input.cpp.cLanguageVersion, knownCLanguageValues, "C");
                if (cLanguageVersion)
                    args.push("-std=" + cLanguageVersion);
            } else if (tag === "cpp") {
                // C++ language version flags.
                var knownCppLanguageValues = ["c++17", "c++14", "c++11", "c++03", "c++98"];
                var cppLanguageVersion = Cpp.languageVersion(
                            input.cpp.cxxLanguageVersion, knownCppLanguageValues, "C++");
                if (cppLanguageVersion)
                    args.push("-std=" + cppLanguageVersion);

                // Exceptions flags.
                var enableExceptions = input.cpp.enableExceptions;
                if (enableExceptions !== undefined)
                    args.push(enableExceptions ? "-fexceptions" : "-fno-exceptions");

                // RTTI flags.
                var enableRtti = input.cpp.enableRtti;
                if (enableRtti !== undefined)
                    args.push(enableRtti ? "-frtti" : "-fno-rtti");
            }

            // Listing files generation does not supported!
            // There are only a workaround: http://www.keil.com/support/docs/4152.htm
        }
    }

    // Misc flags.
    args = args.concat(Cpp.collectMiscCompilerArguments(input, tag),
                       Cpp.collectMiscDriverArguments(input));
    return args;
}

function assemblerFlags(project, product, input, outputs, explicitlyDependsOn) {
    var args = [];
    var architecture = input.qbs.architecture;
    if (isMcsArchitecture(architecture) || isC166Architecture(architecture)) {
        // Input.
        args.push(input.filePath);

        // Output.
        args.push("OBJECT(" + FileInfo.toWindowsSeparators(outputs.obj[0].filePath) + ")");

        // Includes.
        var allIncludePaths = [].concat(Cpp.collectIncludePaths(input),
                                        Cpp.collectSystemIncludePaths(input));
        if (allIncludePaths.length > 0)
            args = args.concat("INCDIR(" + allIncludePaths.map(function(path) {
                return FileInfo.toWindowsSeparators(path); }).join(";") + ")");

        // Debug information flags.
        if (input.cpp.debugInformation)
            args.push("DEBUG");

        // Enable errors printing.
        args.push("EP");

        // Listing files generation flag.
        if (!input.cpp.generateAssemblerListingFiles)
            args.push("NOPRINT");
        else
            args.push("PRINT(" + FileInfo.toWindowsSeparators(outputs.lst[0].filePath) + ")");
    } else if (isArmArchitecture(architecture)) {
        // Input.
        args.push(input.filePath);

        // Output.
        args.push("-o", outputs.obj[0].filePath);

        // Includes.
        args = args.concat(Cpp.collectIncludePathsArguments(input));
        args = args.concat(Cpp.collectSystemIncludePathsArguments(input));

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
        if (input.cpp.generateAssemblerListingFiles)
            args.push("--list", outputs.lst[0].filePath);
    }

    // Misc flags.
    args = args.concat(Cpp.collectMiscAssemblerArguments(input, "asm"));
    return args;
}

function disassemblerFlags(project, product, input, outputs, explicitlyDependsOn) {
    var args = ["--disassemble", "--interleave=source"];
    args.push(outputs.obj[0].filePath);
    args.push("--output=" + outputs.lst[0].filePath);
    return args;
}

function linkerFlags(project, product, inputs, outputs) {
    var args = [];

    var libraryPaths = Cpp.collectLibraryPaths(product);

    var architecture = product.qbs.architecture;
    if (isMcsArchitecture(architecture) || isC166Architecture(architecture)) {
        // Semi-intelligent handling the library paths.
        // We need to add the full path prefix to the library file if this
        // file is not absolute or not relative. Reason is that the C51, C251,
        // and C166 linkers does not support the library paths.
        function collectLibraryObjectPaths(product) {
            var libraryObjects = Cpp.collectLibraryDependencies(product);
            return libraryObjects.map(function(dep) {
                var filePath = dep.filePath;
                if (FileInfo.isAbsolutePath(filePath))
                    return filePath;
                for (var i = 0; i < libraryPaths.length; ++i) {
                    var fullPath = FileInfo.joinPaths(libraryPaths[i], filePath);
                    if (File.exists(fullPath))
                        return fullPath;
                }
                return filePath;
            });
        }

        // Note: The C51, C251, or C166 linker does not distinguish an object files and
        // a libraries, it interpret all this stuff as an input objects,
        // so, we need to pass it together in one string.
        function collectAllObjectPathsArguments(product, inputs) {
            return [].concat(Cpp.collectLinkerObjectPaths(inputs),
                             collectLibraryObjectPaths(product));
        }

        // Add all input objects as arguments (application and library object files).
        var allObjectPaths = collectAllObjectPathsArguments(product, inputs);
        if (allObjectPaths.length > 0)
            args = args.concat(allObjectPaths.map(function(path) {
                return FileInfo.toWindowsSeparators(path); }).join(","));

        // Output.
        args.push("TO", FileInfo.toWindowsSeparators(outputs.application[0].filePath));

        // Map file generation flag.
        if (!product.cpp.generateLinkerMapFile)
            args.push("NOPRINT");
        else
            args.push("PRINT(" + FileInfo.toWindowsSeparators(outputs.mem_map[0].filePath) + ")");
    } else if (isArmArchitecture(architecture)) {
        // Inputs.
        args = args.concat(Cpp.collectLinkerObjectPaths(inputs));

        // Output.
        args.push("--output", outputs.application[0].filePath);

        // Library paths.
        if (libraryPaths.length > 0)
            args.push(product.cpp.libraryPathFlag + libraryPaths.join(","));

        // Library dependencies.
        args = args.concat(Cpp.collectLibraryDependenciesArguments(product));

        // Linker scripts.
        args = args.concat(Cpp.collectLinkerScriptPathsArguments(product, inputs, true));

        // Map file generation flag.
        if (product.cpp.generateLinkerMapFile)
            args.push("--list", outputs.mem_map[0].filePath);

        // Entry point flag.
        if (product.cpp.entryPoint)
            args.push("--entry", product.cpp.entryPoint);

        // Debug information flag.
        var debugInformation = product.cpp.debugInformation;
        if (debugInformation !== undefined)
            args.push(debugInformation ? "--debug" : "--no_debug");
    }

    // Misc flags.
    args = args.concat(Cpp.collectMiscEscapableLinkerArguments(product),
                       Cpp.collectMiscLinkerArguments(product),
                       Cpp.collectMiscDriverArguments(product));
    return args;
}

function archiverFlags(project, product, inputs, outputs) {
    var args = [];

    // Inputs.
    var objectPaths = Cpp.collectLinkerObjectPaths(inputs);

    var architecture = product.qbs.architecture;
    if (isMcsArchitecture(architecture) || isC166Architecture(architecture)) {
        // Library creation command.
        args.push("TRANSFER");

        // Inputs.
        if (objectPaths.length > 0)
            args = args.concat(objectPaths.map(function(path) {
                return FileInfo.toWindowsSeparators(path); }).join(","));

        // Output.
        args.push("TO", FileInfo.toWindowsSeparators(outputs.staticlibrary[0].filePath));
    } else if (isArmArchitecture(architecture)) {
        // Note: The ARM archiver command line expect the output file
        // first, and then a set of input objects.

        // Output.
        args.push("--create", outputs.staticlibrary[0].filePath);

        // Inputs.
        args = args.concat(objectPaths);

        // Debug information flag.
        if (product.cpp.debugInformation)
            args.push("--debug_symbols");
    }

    return args;
}

// The ARMCLANG compiler does not support generation
// for the listing files:
// * https://www.keil.com/support/docs/4152.htm
// So, we generate the listing files from the object files
// using the disassembler.
function generateClangCompilerListing(project, product, inputs, outputs, input, output) {
    if (isArmClangCompiler(input.cpp.compilerPath) && input.cpp.generateCompilerListingFiles) {
        var args = disassemblerFlags(project, product, input, outputs, explicitlyDependsOn);
        var disassemblerPath = input.cpp.disassemblerPath;
        var cmd = new Command(disassemblerPath, args);
        cmd.silent = true;
        return cmd;
    }
}

// The ARMCC compiler generates the listing files only in a short form,
// e.g. to 'module.lst' instead of 'module.{c|cpp}.lst', that complicates
// the auto-tests. Therefore we need to rename generated listing files
// with correct unified names.
function generateArmccCompilerListing(project, product, inputs, outputs, input, output) {
    if (isArmCCCompiler(input.cpp.compilerPath) && input.cpp.generateCompilerListingFiles) {
        var listingPath = FileInfo.path(outputs.lst[0].filePath);
        var cmd = new JavaScriptCommand();
        cmd.oldListing = FileInfo.joinPaths(listingPath, input.baseName + ".lst");
        cmd.newListing = FileInfo.joinPaths(
                    listingPath, input.fileName + input.cpp.compilerListingSuffix);
        cmd.silent = true;
        cmd.sourceCode = function() { File.move(oldListing, newListing); };
        return cmd;
    }
}

function prepareCompiler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var cmds = [];
    var args = compilerFlags(project, product, input, outputs, explicitlyDependsOn);
    var compilerPath = input.cpp.compilerPath;
    var architecture = input.cpp.architecture;
    var cmd = new Command(compilerPath, args);
    cmd.description = "compiling " + input.fileName;
    cmd.highlight = "compiler";
    cmd.jobPool = "compiler";
    if (isMcsArchitecture(architecture)) {
        cmd.maxExitCode = 1;
        cmd.stdoutFilterFunction = filterMcsOutput;
    } else if (isC166Architecture(architecture)) {
        cmd.maxExitCode = 1;
        cmd.stdoutFilterFunction = filterC166Output;
    }
    cmds.push(cmd);

    cmd = generateClangCompilerListing(project, product, inputs, outputs, input, output);
    if (cmd)
        cmds.push(cmd);

    cmd = generateArmccCompilerListing(project, product, inputs, outputs, input, output);
    if (cmd)
        cmds.push(cmd);

    return cmds;
}

function prepareAssembler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var args = assemblerFlags(project, product, input, outputs, explicitlyDependsOn);
    var assemblerPath = input.cpp.assemblerPath;
    var architecture = input.cpp.architecture;
    var cmd = new Command(assemblerPath, args);
    cmd.description = "assembling " + input.fileName;
    cmd.highlight = "compiler";
    cmd.jobPool = "assembler";
    if (isMcsArchitecture(architecture)) {
        cmd.maxExitCode = 1;
        cmd.stdoutFilterFunction = filterMcsOutput;
    } else if (isC166Architecture(architecture)) {
        cmd.maxExitCode = 1;
        cmd.stdoutFilterFunction = filterC166Output;
    }
    return [cmd];
}

function prepareLinker(project, product, inputs, outputs, input, output) {
    var primaryOutput = outputs.application[0];
    var args = linkerFlags(project, product, inputs, outputs);
    var linkerPath = product.cpp.linkerPath;
    var architecture = product.cpp.architecture;
    var cmd = new Command(linkerPath, args);
    cmd.description = "linking " + primaryOutput.fileName;
    cmd.highlight = "linker";
    cmd.jobPool = "linker";
    if (isMcsArchitecture(architecture)) {
        cmd.maxExitCode = 1;
        cmd.stdoutFilterFunction = filterMcsOutput;
    } else if (isC166Architecture(architecture)) {
        cmd.maxExitCode = 1;
        cmd.stdoutFilterFunction = filterC166Output;
    }
    return [cmd];
}

function prepareArchiver(project, product, inputs, outputs, input, output) {
    var args = archiverFlags(project, product, inputs, outputs);
    var archiverPath = product.cpp.archiverPath;
    var architecture = product.cpp.architecture;
    var cmd = new Command(archiverPath, args);
    cmd.description = "creating " + output.fileName;
    cmd.highlight = "linker";
    cmd.jobPool = "linker";
    if (isMcsArchitecture(architecture)) {
        cmd.stdoutFilterFunction = filterMcsOutput;
    } else if (isC166Architecture(architecture)) {
        cmd.stdoutFilterFunction = filterC166Output;
    }
    return [cmd];
}
