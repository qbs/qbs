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

function compilerName(qbs) {
    var architecture = qbs.architecture;
    if (architecture.startsWith("arm"))
        return  "iccarm";
    else if (architecture === "78k")
        return "icc78k";
    else if (architecture === "avr")
        return "iccavr";
    else if (architecture === "avr32")
        return "iccavr32";
    else if (architecture === "cr16")
        return "icccr16c";
    else if (architecture === "hcs12")
        return "icchcs12";
    else if (architecture === "hcs8")
        return "iccs08";
    else if (architecture === "m16c")
        return "iccm16c";
    else if (architecture === "m32c")
        return "iccm32c";
    else if (architecture === "m68k")
        return "icccf";
    else if (architecture === "mcs51")
        return "icc8051";
    else if (architecture === "msp430")
        return "icc430";
    else if (architecture === "r32c")
        return "iccr32c";
    else if (architecture === "rh850")
        return "iccrh850";
    else if (architecture === "riscv")
        return "iccriscv";
    else if (architecture === "rl78")
        return "iccrl78";
    else if (architecture === "rx")
        return "iccrx";
    else if (architecture === "sh")
        return "iccsh";
    else if (architecture === "stm8")
        return "iccstm8";
    else if (architecture === "v850")
        return "iccv850";
    throw "Unable to deduce compiler name for unsupported architecture: '"
            + architecture + "'";
}

function assemblerName(qbs) {
    var architecture = qbs.architecture;
    if (architecture.startsWith("arm"))
        return "iasmarm";
    else if (architecture === "78k")
        return "a78k";
    else if (architecture === "avr")
        return "aavr";
    else if (architecture === "avr32")
        return "aavr32";
    else if (architecture === "cr16")
        return "acr16c";
    else if (architecture === "hcs12")
        return "ahcs12";
    else if (architecture === "hcs8")
        return "as08";
    else if (architecture === "m16c")
        return "am16c";
    else if (architecture === "m32c")
        return "am32c";
    else if (architecture === "m68k")
        return "acf";
    else if (architecture === "mcs51")
        return "a8051";
    else if (architecture === "msp430")
        return "a430";
    else if (architecture === "r32c")
        return "ar32c";
    else if (architecture === "rh850")
        return "iasmrh850";
    else if (architecture === "riscv")
        return "iasmriscv";
    else if (architecture === "rl78")
        return "iasmrl78";
    else if (architecture === "rx")
        return "iasmrx";
    else if (architecture === "sh")
        return "iasmsh";
    else if (architecture === "stm8")
        return "iasmstm8";
    else if (architecture === "v850")
        return "av850";
    throw "Unable to deduce assembler name for unsupported architecture: '"
            + architecture + "'";
}

function linkerName(qbs) {
    var architecture = qbs.architecture;
    if (supportXLinker(architecture))
        return "xlink";
    else if (supportILinker(architecture))
        return architecture.startsWith("arm") ? "ilinkarm" : ("ilink" + architecture);
    throw "Unable to deduce linker name for unsupported architecture: '"
            + architecture + "'";
}

function archiverName(qbs) {
    var architecture = qbs.architecture;
    if (supportXArchiver(architecture))
        return "xar";
    else if (supportIArchiver(architecture))
        return "iarchive";
    throw "Unable to deduce archiver name for unsupported architecture: '"
            + architecture + "'";
}

function staticLibrarySuffix(qbs) {
    var architecture = qbs.architecture;
    var code = architectureCode(architecture);
    if (code === undefined) {
        throw "Unable to deduce static library suffix for unsupported architecture: '"
                + architecture + "'";
    }
    return (code !== "") ? (".r" + code) : ".a";
}

function executableSuffix(qbs) {
    var architecture = qbs.architecture;
    var code = architectureCode(architecture);
    if (code === undefined) {
        throw "Unable to deduce executable suffix for unsupported architecture: '"
                + architecture + "'";
    }
    return (code !== "") ? ((qbs.debugInformation) ? (".d" + code) : (".a" + code)) : ".out";
}

function objectSuffix(qbs) {
    var architecture = qbs.architecture;
    var code = architectureCode(architecture);
    if (code === undefined) {
        throw "Unable to deduce object file suffix for unsupported architecture: '"
                + architecture + "'";
    }
    return (code !== "") ? (".r" + code) : ".o";
}

function imageFormat(qbs) {
    var architecture = qbs.architecture;
    var code = architectureCode(architecture);
    if (code === undefined) {
        throw "Unable to deduce image format for unsupported architecture: '"
                + architecture + "'";
    }
    return (code !== "") ? "ubrof" : "elf";
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
    throw "Unable to deduce C++ language option for unsupported compiler: '"
            + FileInfo.toNativeSeparators(compilerFilePath) + "'";
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
    p.exec(compilerFilePath, args, true);
    var outFile = new TextFile(outFilePath, TextFile.ReadOnly);
    return ModUtils.extractMacros(outFile.readAll());
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
    // It is possible that the process can return an error code in case the
    // compiler does not support the `--IDE3` flag. So, don't throw an error in this case.
    p.exec(compilerFilePath, args, false);
    p.readStdOut().trim().split(/\r?\n/g).map(function(line) {
        var m = line.match(/\$\$INC_BEGIN\s\$\$FILEPATH\s\"([^"]*)/);
        if (m) {
            var includePath =  m[1].replace(/\\\\/g, '/');
            if (includePath)
                includePaths.push(includePath);
        }
    });

    if (includePaths.length === 0) {
        // This can happen if the compiler does not support the `--IDE3` flag,
        // e.g. IAR for S08 architecture. In this case we use fallback to the
        // detection of the `inc` directory.
        var includePath = FileInfo.joinPaths(FileInfo.path(compilerFilePath), "../inc/");
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
            return (a instanceof Array) ? a : [];
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
    var args = [];

    // Input.
    args.push(input.filePath);

    // Output.
    args.push("-o", outputs.obj[0].filePath);

    var prefixHeaders = input.cpp.prefixHeaders;
    for (var i in prefixHeaders)
        args.push("--preinclude", prefixHeaders[i]);

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
    var distributionIncludePaths = input.cpp.distributionIncludePaths;
    if (distributionIncludePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(distributionIncludePaths);
    args = args.concat(allIncludePaths.map(function(include) { return "-I" + include }));

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
            // Default C language version is C11/C99 that
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

    var architecture = input.qbs.architecture;

    // The `--preinclude` flag is only supported for a certain
    // set of assemblers, not for all.
    if (supportIAssembler(architecture)) {
        var prefixHeaders = input.cpp.prefixHeaders;
        for (var i in prefixHeaders)
            args.push("--preinclude", prefixHeaders[i]);
    }

    // Includes.
    var allIncludePaths = [];
    var systemIncludePaths = input.cpp.systemIncludePaths;
    if (systemIncludePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(systemIncludePaths);
    var distributionIncludePaths = input.cpp.distributionIncludePaths;
    if (distributionIncludePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(distributionIncludePaths);
    args = args.concat(allIncludePaths.map(function(include) { return "-I" + include }));

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
        args.push("--endian=" + endianness);

    // Listing files generation flag.
    if (input.cpp.generateAssemblerListingFiles)
        args.push("-l", outputs.lst[0].filePath);

    // Misc flags.
    args = args.concat(ModUtils.moduleProperty(input, "platformFlags", tag),
                       ModUtils.moduleProperty(input, "flags", tag));
    return args;
}

function linkerFlags(project, product, inputs, outputs) {
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
    if (supportILinker(architecture)) {
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
    } else if (supportXLinker(architecture)) {
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

function archiverFlags(project, product, inputs, outputs) {
    var args = [];

    // Inputs.
    if (inputs.obj)
        args = args.concat(inputs.obj.map(function(obj) { return obj.filePath }));

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
    var args = linkerFlags(project, product, inputs, outputs);
    var linkerPath = product.cpp.linkerPath;
    var cmd = new Command(linkerPath, args);
    cmd.description = "linking " + primaryOutput.fileName;
    cmd.highlight = "linker";
    return [cmd];
}

function prepareArchiver(project, product, inputs, outputs, input, output) {
    var args = archiverFlags(project, product, inputs, outputs);
    var archiverPath = product.cpp.archiverPath;
    var cmd = new Command(archiverPath, args);
    cmd.description = "linking " + output.fileName;
    cmd.highlight = "linker";
    cmd.stdoutFilterFunction = function(output) {
        return "";
    };
    return [cmd];
}
