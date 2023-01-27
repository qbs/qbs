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
    return architecture === "78k"
            || architecture === "avr"
            || architecture === "avr32"
            || architecture === "cr16"
            || architecture === "hcs12"
            || architecture === "hcs8"
            || architecture === "m16c"
            || architecture === "m32c"
            || architecture === "m68k"
            || architecture === "mcs51"
            || architecture === "msp430"
            || architecture === "r32c"
            || architecture === "v850";
}

function supportILinker(architecture) {
    return architecture.startsWith("arm")
            || architecture === "rh850"
            || architecture === "riscv"
            || architecture === "rl78"
            || architecture === "rx"
            || architecture === "sh"
            || architecture === "stm8";
}

function supportXArchiver(architecture) {
    return architecture === "78k"
            || architecture === "avr"
            || architecture === "avr32"
            || architecture === "cr16"
            || architecture === "hcs12"
            || architecture === "hcs8"
            || architecture === "m16c"
            || architecture === "m32c"
            || architecture === "m68k"
            || architecture === "mcs51"
            || architecture === "msp430"
            || architecture === "r32c"
            || architecture === "v850";
}

function supportIArchiver(architecture) {
    return architecture.startsWith("arm")
            || architecture === "rh850"
            || architecture === "riscv"
            || architecture === "rl78"
            || architecture === "rx"
            || architecture === "sh"
            || architecture === "stm8";
}

function supportXAssembler(architecture) {
    return architecture.startsWith("arm")
            || architecture === "78k"
            || architecture === "avr"
            || architecture === "hcs12"
            || architecture === "m16c"
            || architecture === "mcs51"
            || architecture === "msp430"
            || architecture === "m32c"
            || architecture === "v850";
}

function supportIAssembler(architecture) {
    return architecture === "avr32"
            || architecture === "cr16"
            || architecture === "hcs8"
            || architecture === "r32c"
            || architecture === "rh850"
            || architecture === "riscv"
            || architecture === "rl78"
            || architecture === "rx"
            || architecture === "sh"
            || architecture === "stm8"
            || architecture === "m68k";
}

function supportEndianness(architecture) {
    return architecture.startsWith("arm")
            || architecture === "rx";
}

function supportCppExceptions(architecture) {
    return architecture.startsWith("arm")
            || architecture === "rh850"
            || architecture === "riscv"
            || architecture === "rl78"
            || architecture === "rx";
}

function supportCppRtti(architecture) {
    return architecture.startsWith("arm")
            || architecture === "rh850"
            || architecture === "riscv"
            || architecture === "rl78"
            || architecture === "rx";
}

function supportCppWarningAboutCStyleCast(architecture) {
    return architecture.startsWith("arm")
            || architecture === "avr"
            || architecture === "avr32"
            || architecture === "cr16"
            || architecture === "mcs51"
            || architecture === "msp430"
            || architecture === "rh850"
            || architecture === "rl78"
            || architecture === "rx"
            || architecture === "stm8"
            || architecture === "v850";
}

function supportDeprecatedFeatureWarnings(architecture) {
    return architecture.startsWith("arm")
            || architecture === "avr"
            || architecture === "cr16"
            || architecture === "mcs51"
            || architecture === "msp430"
            || architecture === "rh850"
            || architecture === "rl78"
            || architecture === "rx"
            || architecture === "stm8"
            || architecture === "v850";
}

function supportCLanguageVersion(architecture) {
    return architecture !== "78k";
}

function supportCppLanguage(compilerFilePath) {
    var baseName = FileInfo.baseName(compilerFilePath);
    return baseName !== "iccs08";
}

// It is a 'magic' IAR-specific target architecture code.
function architectureCode(architecture) {
    switch (architecture) {
    case "78k":
        return "26";
    case "avr":
        return "90";
    case "avr32":
        return "82";
    case "mcs51":
        return "51";
    case "msp430":
        return "43";
    case "v850":
        return "85";
    case "m68k":
        return "68";
    case "m32c":
        return "48";
    case "r32c":
        return "53";
    case "m16c":
        return "34";
    case "cr16":
        return "45";
    case "hcs12":
        return "12";
    case "hcs8":
        return "78";
    case "rh850": case "riscv": case "rl78": case "rx": case "sh": case "stm8":
        return "";
    default:
        if (architecture.startsWith("arm"))
            return "";
        break;
    }
}

function toolchainDetails(qbs) {
    var architecture = qbs.architecture;
    var code = architectureCode(architecture);
    var details = {};

    if (supportXLinker(architecture)) {
        details.libraryPathFlag = "-I";
        details.linkerScriptFlag = "-f";
        details.linkerName = "xlink";
        details.linkerSilentFlag = "-S";
        details.linkerMapFileFlag = "-l";
        details.linkerEntryPointFlag = "-s";
    } else if (supportILinker(architecture)) {
        details.libraryPathFlag = "-L";
        details.linkerScriptFlag = "--config";
        details.linkerSilentFlag = "--silent";
        details.linkerMapFileFlag = "--map";
        details.linkerEntryPointFlag = "--entry";
        details.linkerName = architecture.startsWith("arm")
                ? "ilinkarm" : ("ilink" + architecture);
    }

    if (supportXArchiver(architecture))
        details.archiverName = "xar";
    else if (supportIArchiver(architecture))
        details.archiverName = "iarchive";

    var hasCode = (code !== "");
    details.staticLibrarySuffix = hasCode ? (".r" + code) : ".a";
    details.executableSuffix = hasCode
            ? ((qbs.debugInformation) ? (".d" + code) : (".a" + code)) : ".out";
    details.objectSuffix = hasCode ? (".r" + code) : ".o";
    details.imageFormat = hasCode ? "ubrof" : "elf";

    if (architecture.startsWith("arm")) {
        details.compilerName = "iccarm";
        details.assemblerName = "iasmarm";
    } else if (architecture === "78k") {
        details.compilerName = "icc78k";
        details.assemblerName = "a78k";
    } else if (architecture === "avr") {
        details.compilerName = "iccavr";
        details.assemblerName = "aavr";
    } else if (architecture === "avr32") {
        details.compilerName = "iccavr32";
        details.assemblerName = "aavr32";
    } else if (architecture === "cr16") {
        details.compilerName = "icccr16c";
        details.assemblerName = "acr16c";
    } else if (architecture === "hcs12") {
        details.compilerName = "icchcs12";
        details.assemblerName = "ahcs12";
    } else if (architecture === "hcs8") {
        details.compilerName = "iccs08";
        details.assemblerName = "as08";
    } else if (architecture === "m16c") {
        details.compilerName = "iccm16c";
        details.assemblerName = "am16c";
    } else if (architecture === "m32c") {
        details.compilerName = "iccm32c";
        details.assemblerName = "am32c";
    } else if (architecture === "m68k") {
        details.compilerName = "icccf";
        details.assemblerName = "acf";
    } else if (architecture === "mcs51") {
        details.compilerName = "icc8051";
        details.assemblerName = "a8051";
    } else if (architecture === "msp430") {
        details.compilerName = "icc430";
        details.assemblerName = "a430";
    } else if (architecture === "r32c") {
        details.compilerName = "iccr32c";
        details.assemblerName = "ar32c";
    } else if (architecture === "rh850") {
        details.compilerName = "iccrh850";
        details.assemblerName = "iasmrh850";
    } else if (architecture === "riscv") {
        details.compilerName = "iccriscv";
        details.assemblerName = "iasmriscv";
    } else if (architecture === "rl78") {
        details.compilerName = "iccrl78";
        details.assemblerName = "iasmrl78";
    } else if (architecture === "rx") {
        details.compilerName = "iccrx";
        details.assemblerName = "iasmrx";
    } else if (architecture === "sh") {
        details.compilerName = "iccsh";
        details.assemblerName = "iasmsh";
    } else if (architecture === "stm8") {
        details.compilerName = "iccstm8";
        details.assemblerName = "iasmstm8";
    } else if (architecture === "v850") {
        details.compilerName = "iccv850";
        details.assemblerName = "av850";
    }

    return details;
}

function guessArmArchitecture(core) {
    var arch = "arm";
    if (core === "__ARM4M__")
        arch += "v4m";
    else if (core === "__ARM4TM__")
        arch += "v4tm";
    else if (core === "__ARM5E__")
        arch += "v5e";
    else if (core === "__ARM5__")
        arch += "v5";
    else if (core === "__ARM6M__")
        arch += "v6m";
    else if (core === "__ARM6SM__")
        arch += "v6sm";
    else if (core === "__ARM6__")
        arch += "v6";
    else if (core === "__ARM7M__")
        arch += "v7m";
    else if (core === "__ARM7R__")
        arch += "v7r";
    return arch;
}

function guessArchitecture(macros) {
    if (macros["__ICC430__"] === "1")
        return "msp430";
    else if (macros["__ICC78K__"] === "1")
        return "78k";
    else if (macros["__ICC8051__"] === "1")
        return "mcs51";
    else if (macros["__ICCARM__"] === "1")
        return guessArmArchitecture(macros["__CORE__"]);
    else if (macros["__ICCAVR32__"] === "1")
        return "avr32";
    else if (macros["__ICCAVR__"] === "1")
        return "avr";
    else if (macros["__ICCCF__"] === "1")
        return "m68k";
    else if (macros["__ICCCR16C__"] === "1")
        return "cr16";
    else if (macros["__ICCHCS12__"] === "1")
        return "hcs12";
    else if (macros["__ICCM16C__"] === "1")
        return "m16c";
    else if (macros["__ICCM32C__"] === "1")
        return "m32c";
    else if (macros["__ICCR32C__"] === "1")
        return "r32c";
    else if (macros["__ICCRH850__"] === "1")
        return "rh850";
    else if (macros["__ICCRISCV__"] === "1")
        return "riscv";
    else if (macros["__ICCRL78__"] === "1")
        return "rl78";
    else if (macros["__ICCRX__"] === "1")
        return "rx";
    else if (macros["__ICCS08__"] === "1")
        return "hcs8";
    else if (macros["__ICCSH__"] === "1")
        return "sh";
    else if (macros["__ICCSTM8__"] === "1")
        return "stm8";
    else if (macros["__ICCV850__"] === "1")
        return "v850";
}

function guessEndianness(macros) {
    if (macros["__LITTLE_ENDIAN__"] === "1")
        return "little";
    return "big"
}

function guessVersion(macros, architecture) {
    var version = parseInt(macros["__VER__"], 10);
    if (architecture.startsWith("arm")) {
        return { major: parseInt(version / 1000000),
            minor: parseInt(version / 1000) % 1000,
            patch: parseInt(version) % 1000 }
    } else if (architecture === "78k"
               || architecture === "avr"
               || architecture === "avr32"
               || architecture === "cr16"
               || architecture === "hcs12"
               || architecture === "hcs8"
               || architecture === "m16c"
               || architecture === "m32c"
               || architecture === "m68k"
               || architecture === "mcs51"
               || architecture === "msp430"
               || architecture === "r32c"
               || architecture === "rh850"
               || architecture === "riscv"
               || architecture === "rl78"
               || architecture === "rx"
               || architecture === "sh"
               || architecture === "stm8"
               || architecture === "v850") {
        return { major: parseInt(version / 100),
            minor: parseInt(version % 100),
            patch: 0 }
    }
}

function cppLanguageOption(compilerFilePath) {
    var baseName = FileInfo.baseName(compilerFilePath);
    switch (baseName) {
    case "iccarm":
    case "iccrh850":
    case "iccriscv":
    case "iccrl78":
    case "iccrx":
        return "--c++";
    case "icc430":
    case "icc78k":
    case "icc8051":
    case "iccavr":
    case "iccavr32":
    case "icccf":
    case "icccr16c":
    case "icchcs12":
    case "iccm16c":
    case "iccm32c":
    case "iccr32c":
    case "iccsh":
    case "iccstm8":
    case "iccv850":
        return "--ec++";
    }
}

function dumpMacros(compilerFilePath, tag) {
    var tempDir = new TemporaryDir();
    var inFilePath = FileInfo.fromNativeSeparators(tempDir.path() + "/empty-source.c");
    var inFile = new TextFile(inFilePath, TextFile.WriteOnly);
    var outFilePath = FileInfo.fromNativeSeparators(tempDir.path() + "/iar-macros.predef");

    var args = [ inFilePath, "--predef_macros", outFilePath ];
    if (tag === "cpp" && supportCppLanguage(compilerFilePath))
        args.push(cppLanguageOption(compilerFilePath));

    var p = new Process();
    p.setWorkingDirectory(tempDir.path());
    p.exec(compilerFilePath, args, true);
    var outFile = new TextFile(outFilePath, TextFile.ReadOnly);
    return Cpp.extractMacros(outFile.readAll());
}

function dumpCompilerIncludePaths(compilerFilePath, tag) {
    // We can dump the compiler include paths using the undocumented `--IDE3` flag,
    // e.g. which also is used in the IAR extension for the VSCode. In this case the
    // compiler procuces the console output in the following format:
    // `$$TOOL_BEGIN $$VERSION "3" $$INC_BEGIN $$FILEPATH "<path\\to\\directory>" $$TOOL_END`

    var tempDir = new TemporaryDir();
    var inFilePath = FileInfo.fromNativeSeparators(tempDir.path() + "/empty-source.c");
    var inFile = new TextFile(inFilePath, TextFile.WriteOnly);

    var args = ["--IDE3", inFilePath];
    if (tag === "cpp" && supportCppLanguage(compilerFilePath))
        args.push(cppLanguageOption(compilerFilePath));

    var includePaths = [];
    var p = new Process();
    p.setWorkingDirectory(tempDir.path());
    // It is possible that the process can return an error code in case the
    // compiler does not support the `--IDE3` flag. So, don't throw an error in this case.
    p.exec(compilerFilePath, args, false);
    p.readStdOut().trim().split(/\r?\n/g).map(function(line) {
        var m = line.match(/\$\$INC_BEGIN\s\$\$FILEPATH\s\"([^"]*)/);
        if (m) {
            var includePath =  m[1].replace(/\\\\/g, '/');
            if (includePath && File.exists(includePath))
                includePaths.push(includePath);
        }
    });

    if (includePaths.length === 0) {
        // This can happen if the compiler does not support the `--IDE3` flag,
        // e.g. IAR for S08 architecture. In this case we use fallback to the
        // detection of the `inc` directory.
        var includePath = FileInfo.joinPaths(FileInfo.path(compilerFilePath), "../inc/");
        if (File.exists(includePath))
            includePaths.push(includePath);
    }

    return includePaths;
}

function dumpDefaultPaths(compilerFilePath, tag) {
    var includePaths = dumpCompilerIncludePaths(compilerFilePath, tag);
    return {
        "includePaths": includePaths
    };
}

function compilerFlags(project, product, input, outputs, explicitlyDependsOn) {
    var args = [];

    // Input.
    args.push(input.filePath);

    // Output.
    args.push("-o", outputs.obj[0].filePath);

    // Preinclude headers.
    args = args.concat(Cpp.collectPreincludePathsArguments(input, true));

    // Defines.
    args = args.concat(Cpp.collectDefinesArguments(input));

    // Includes.
    args = args.concat(Cpp.collectIncludePathsArguments(input));
    args = args.concat(Cpp.collectSystemIncludePathsArguments(input));

    // Silent output generation flag.
    args.push("--silent");

    // Debug information flags.
    if (input.cpp.debugInformation)
        args.push("--debug");

    // Optimization flags.
    switch (input.cpp.optimization) {
    case "small":
        args.push("-Ohz");
        break;
    case "fast":
        args.push("-Ohs");
        break;
    case "none":
        args.push("-On");
        break;
    }

    var architecture = input.qbs.architecture;
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(outputs.obj[0].fileTags));

    // Warning level flags.
    switch (input.cpp.warningLevel) {
    case "none":
        args.push("--no_warnings");
        break;
    case "all":
            if (supportDeprecatedFeatureWarnings(architecture)) {
                args.push("--deprecated_feature_warnings="
                    +"+attribute_syntax,"
                    +"+preprocessor_extensions,"
                    +"+segment_pragmas");
            }
            if (tag === "cpp" && supportCppWarningAboutCStyleCast(architecture))
                args.push("--warn_about_c_style_casts");
        break;
    }
    if (input.cpp.treatWarningsAsErrors)
        args.push("--warnings_are_errors");

    // C language version flags.
    if (tag === "c" && supportCLanguageVersion(architecture)) {
        var knownValues = ["c89"];
        var cLanguageVersion = Cpp.languageVersion(
                    input.cpp.cLanguageVersion, knownValues, "C");
        switch (cLanguageVersion) {
        case "c89":
            args.push("--c89");
            break;
        default:
            // Default C language version is C18/C11/C99 that
            // depends on the IAR version.
            break;
        }
    }

    // C++ language version flags.
    var compilerFilePath = input.cpp.compilerPath;
    if (tag === "cpp" && supportCppLanguage(compilerFilePath)) {
        // C++ language flag.
        var cppOption = cppLanguageOption(compilerFilePath);
        args.push(cppOption);

        // Exceptions flag.
        var enableExceptions = input.cpp.enableExceptions;
        if (!enableExceptions && supportCppExceptions(architecture))
            args.push("--no_exceptions");

        // RTTI flag.
        var enableRtti = input.cpp.enableRtti;
        if (!enableRtti && supportCppRtti(architecture))
            args.push("--no_rtti");
    }

    // Byte order flags.
    var endianness = input.cpp.endianness;
    if (endianness && supportEndianness(architecture))
        args.push("--endian=" + endianness);

    // Listing files generation flag.
    if (input.cpp.generateCompilerListingFiles)
        args.push("-l", outputs.lst[0].filePath);

    // Misc flags.
    args = args.concat(Cpp.collectMiscCompilerArguments(input, tag),
                       Cpp.collectMiscDriverArguments(product));
    return args;
}

function assemblerFlags(project, product, input, outputs, explicitlyDependsOn) {
    var args = [];

    // Input.
    args.push(input.filePath);

    // Output.
    args.push("-o", outputs.obj[0].filePath);

    var architecture = input.qbs.architecture;

    // The `--preinclude` flag is only supported for a certain
    // set of assemblers, not for all.
    if (supportIAssembler(architecture))
        args = args.concat(Cpp.collectPreincludePathsArguments(input));

    // Includes.
    args = args.concat(Cpp.collectIncludePathsArguments(input));
    args = args.concat(Cpp.collectSystemIncludePathsArguments(input));

    // Debug information flags.
    if (input.cpp.debugInformation)
        args.push("-r");

    // Architecture specific flags.
    if (supportIAssembler(architecture)) {
        // Silent output generation flag.
        args.push("--silent");
        // Warning level flags.
        if (input.cpp.warningLevel === "none")
            args.push("--no_warnings");
        if (input.cpp.treatWarningsAsErrors)
            args.push("--warnings_are_errors");
    } else if (supportXAssembler(architecture)){
        // Silent output generation flag.
        args.push("-S");
        // Warning level flags.
        args.push("-w" + (input.cpp.warningLevel === "none" ? "-" : "+"));
    }

    // Byte order flags.
    var endianness = input.cpp.endianness;
    if (endianness && supportEndianness(architecture))
        args.push("--endian", endianness);

    // Listing files generation flag.
    if (input.cpp.generateAssemblerListingFiles)
        args.push("-l", outputs.lst[0].filePath);

    // Misc flags.
    args = args.concat(Cpp.collectMiscAssemblerArguments(input, "asm"));
    return args;
}

function linkerFlags(project, product, inputs, outputs) {
    var args = [];

    // Inputs.
    args = args.concat(Cpp.collectLinkerObjectPaths(inputs));

    // Output.
    args.push("-o", outputs.application[0].filePath);

    // Library paths.
    args = args.concat(Cpp.collectLibraryPathsArguments(product));

    // Library dependencies.
    args = args.concat(Cpp.collectLibraryDependenciesArguments(product));

    // Linker scripts.
    args = args.concat(Cpp.collectLinkerScriptPathsArguments(product, inputs, true));

    // Silent output generation flag.
    args.push(product.cpp.linkerSilentFlag);

    // Map file generation flag.
    if (product.cpp.generateLinkerMapFile)
        args.push(product.cpp.linkerMapFileFlag, outputs.mem_map[0].filePath);

    // Entry point flag.
    if (product.cpp.entryPoint)
        args.push(product.cpp.linkerEntryPointFlag, product.cpp.entryPoint);

    // Debug information flag.
    if (supportXLinker(product.qbs.architecture)) {
        if (product.cpp.debugInformation)
            args.push("-rt");
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
    args = args.concat(Cpp.collectLinkerObjectPaths(inputs));

    // Output.
    var architecture = product.qbs.architecture;
    if (supportIArchiver(architecture))
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
    cmd.jobPool = "compiler";
    return [cmd];
}

function prepareAssembler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var args = assemblerFlags(project, product, input, outputs, explicitlyDependsOn);
    var assemblerPath = input.cpp.assemblerPath;
    var cmd = new Command(assemblerPath, args);
    cmd.description = "assembling " + input.fileName;
    cmd.highlight = "compiler";
    cmd.jobPool = "assembler";
    return [cmd];
}

function prepareLinker(project, product, inputs, outputs, input, output) {
    var primaryOutput = outputs.application[0];
    var args = linkerFlags(project, product, inputs, outputs);
    var linkerPath = product.cpp.linkerPath;
    var cmd = new Command(linkerPath, args);
    cmd.description = "linking " + primaryOutput.fileName;
    cmd.highlight = "linker";
    cmd.jobPool = "linker";
    return [cmd];
}

function prepareArchiver(project, product, inputs, outputs, input, output) {
    var args = archiverFlags(project, product, inputs, outputs);
    var archiverPath = product.cpp.archiverPath;
    var cmd = new Command(archiverPath, args);
    cmd.description = "creating " + output.fileName;
    cmd.highlight = "linker";
    cmd.jobPool = "linker";
    cmd.stdoutFilterFunction = function(output) {
        return "";
    };
    return [cmd];
}
