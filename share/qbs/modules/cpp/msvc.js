/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

var Codesign = require("../codesign/codesign.js");
var Cpp = require("cpp.js");
var File = require("qbs.File");
var FileInfo = require("qbs.FileInfo");
var ModUtils = require("qbs.ModUtils");
var Utilities = require("qbs.Utilities");
var WindowsUtils = require("qbs.WindowsUtils");

function effectiveLinkerPath(product, inputs) {
    if (product.cpp.linkerMode === "automatic") {
        var compiler = product.cpp.compilerPath;
        if (compiler) {
            if (inputs.obj || inputs.staticlibrary) {
                console.log("Found C/C++ objects, using compiler as a linker for " + product.name);
                return compiler;
            }
        }

        console.log("Found no C-language objects, choosing system linker for " + product.name);
    }

    return product.cpp.linkerPath;
}

function handleCpuFeatures(input, flags) {
    if (!input.qbs.architecture)
        return;
    if (input.qbs.architecture.startsWith("x86")) {
        if (input.qbs.architecture === "x86") {
            var sse2 = input.cpufeatures.x86_sse2;
            if (sse2 === true)
                flags.push("/arch:SSE2");
            if (sse2 === false)
                flags.push("/arch:IA32");
        }
        if (input.cpufeatures.x86_avx === true)
            flags.push("/arch:AVX");
        if (input.cpufeatures.x86_avx2 === true)
            flags.push("/arch:AVX2");
    } else if (input.qbs.architecture.startsWith("arm")) {
        if (input.cpufeatures.arm_vfpv4 === true)
            flags.push("/arch:VFPv4");
        if (input.cpp.machineType === "armv7ve")
            flags.push("/arch:ARMv7VE");
    }
}

function hasCxx17Option(input)
{
    // Probably this is not the earliest version to support the flag, but we have tested this one
    // and it's a pain to find out the exact minimum.
    return (input.qbs.toolchain.includes("clang-cl") && input.cpp.compilerVersionMajor >= 7)
        || Utilities.versionCompare(input.cpp.compilerVersion, "19.12.25831") >= 0;
}

function hasZCplusPlusOption(input)
{
    // /Zc:__cplusplus is supported starting from Visual Studio 15.7
    // Looks like closest MSVC version is 14.14.26428 (cl ver 19.14.26433)
    // At least, this version is tested
    // https://docs.microsoft.com/en-us/cpp/build/reference/zc-cplusplus
    // clang-cl supports this option starting around-ish versions 8/9, but it
    // ignores this option, so this doesn't really matter
    // https://reviews.llvm.org/D45877
    return (input.qbs.toolchain.includes("clang-cl") && input.cpp.compilerVersionMajor >= 9)
        || Utilities.versionCompare(input.cpp.compilerVersion, "19.14.26433") >= 0;
}

function hasCxx20Option(input)
{
    return (input.qbs.toolchain.includes("clang-cl") && input.cpp.compilerVersionMajor >= 13)
        || Utilities.versionCompare(input.cpp.compilerVersion, "19.29.30133.0") >= 0;
}

function hasCVerOption(input)
{
    return (input.qbs.toolchain.includes("clang-cl") && input.cpp.compilerVersionMajor >= 13)
        || Utilities.versionCompare(input.cpp.compilerVersion, "19.29.30138.0") >= 0;
}

function supportsExternalIncludesOption(input) {
    if (input.qbs.toolchain.includes("clang-cl"))
        return false; // Exclude clang-cl.
    // This option was introcuded since MSVC 2017 v15.6 (aka _MSC_VER 19.13).
    // But due to some MSVC bugs:
    // * https://developercommunity.visualstudio.com/content/problem/181006/externali-include-paths-not-working.html
    // this option has been fixed since MSVC 2017 update 9, v15.9 (aka _MSC_VER 19.16).
    return Utilities.versionCompare(input.cpp.compilerVersion, "19.16") >= 0;
}

function addCxxLanguageVersionFlag(input, args) {
    var cxxVersion = Cpp.languageVersion(input.cpp.cxxLanguageVersion,
            ["c++23", "c++20", "c++17", "c++14", "c++11", "c++98"], "C++");
    if (!cxxVersion)
        return;

    // Visual C++ 2013, Update 3 or clang-cl
    var hasStdOption = input.qbs.toolchain.includes("clang-cl")
        || Utilities.versionCompare(input.cpp.compilerVersion, "18.00.30723") >= 0;
    if (!hasStdOption)
        return;

    var flag;
    if (cxxVersion === "c++14")
        flag = "/std:c++14";
    else if (cxxVersion === "c++17" && hasCxx17Option(input))
        flag = "/std:c++17";
    else if (cxxVersion === "c++20" && hasCxx20Option(input))
        flag = "/std:c++20";
    else if (cxxVersion !== "c++11" && cxxVersion !== "c++98")
        flag = "/std:c++latest";
    if (flag)
        args.push(flag);
}

function addCLanguageVersionFlag(input, args) {
    var cVersion = Cpp.languageVersion(input.cpp.cLanguageVersion,
            ["c17", "c11"], "C");
    if (!cVersion)
        return;

    var hasStdOption = hasCVerOption(input);
    if (!hasStdOption)
        return;

    var flag;
    if (cVersion === "c17")
        flag = "/std:c17";
    else if (cVersion === "c11")
        flag = "/std:c11";
    if (flag)
        args.push(flag);
}

function handleClangClArchitectureFlags(product, architecture, flags) {
    if (product.qbs.toolchain.includes("clang-cl")) {
        if (architecture === "x86")
            flags.push("-m32");
        else if (architecture === "x86_64")
            flags.push("-m64");
    }
}

function prepareCompiler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var i;
    var debugInformation = input.cpp.debugInformation;
    var args = ['/nologo', '/c']

    handleCpuFeatures(input, args);

    // Determine which C-language we're compiling
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(Object.keys(outputs)));
    if (!["c", "cpp"].includes(tag))
        throw ("unsupported source language");

    var enableExceptions = input.cpp.enableExceptions;
    if (enableExceptions) {
        var ehModel = input.cpp.exceptionHandlingModel;
        switch (ehModel) {
        case "default":
            args.push("/EHsc"); // "Yes" in VS
            break;
        case "seh":
            args.push("/EHa"); // "Yes with SEH exceptions" in VS
            break;
        case "externc":
            args.push("/EHs"); // "Yes with Extern C functions" in VS
            break;
        }
    }

    var enableRtti = input.cpp.enableRtti;
    if (enableRtti !== undefined) {
        args.push(enableRtti ? "/GR" : "/GR-");
    }

    switch (input.cpp.optimization) {
    case "small":
        args.push('/Os')
        break;
    case "fast":
        args.push('/O2')
        break;
    case "none":
        args.push("/Od");
        break;
    }

    handleClangClArchitectureFlags(product, input.cpp.architecture, args);

    if (debugInformation) {
        if (product.cpp.separateDebugInformation)
            args.push('/Zi');
        else
            args.push('/Z7');
    }

    var rtl = product.cpp.runtimeLibrary;
    if (rtl) {
        rtl = (rtl === "static" ? "/MT" : "/MD");
        if (product.qbs.enableDebugCode)
            rtl += "d";
        args.push(rtl);
    }

    args = args.concat(Cpp.collectMiscDriverArguments(product));

    // warnings:
    var warningLevel = input.cpp.warningLevel;
    if (warningLevel === 'none')
        args.push('/w')
    if (warningLevel === 'all')
        args.push('/Wall')
    if (input.cpp.treatWarningsAsErrors)
        args.push('/WX')

    var includePaths = Cpp.collectIncludePaths(input);
    args = args.concat([].uniqueConcat(includePaths).map(function(path) {
        return input.cpp.includeFlag + FileInfo.toWindowsSeparators(path);
    }));

    var includeFlag = input.qbs.toolchain.includes("clang-cl")
        ? input.cpp.systemIncludeFlag : input.cpp.includeFlag;
    if (!input.qbs.toolchain.includes("clang-cl")) {
        if (supportsExternalIncludesOption(input)) {
            args.push("/experimental:external");
            var enforcesSlashW =
                    Utilities.versionCompare(input.cpp.compilerVersion, "19.29.30037") >= 0
            if (enforcesSlashW)
                args.push("/external:W0")
            includeFlag = input.cpp.systemIncludeFlag;
        }
    }
    var systemIncludePaths = Cpp.collectSystemIncludePaths(input);
    args = args.concat([].uniqueConcat(systemIncludePaths).map(function(path) {
        return includeFlag + FileInfo.toWindowsSeparators(path);
    }));

    var defines = Cpp.collectDefines(input);
    args = args.concat([].uniqueConcat(defines).map(function(define) {
        return input.cpp.defineFlag + define.replace(/%/g, "%%");
    }));

    var minimumWindowsVersion = product.cpp.minimumWindowsVersion;
    if (minimumWindowsVersion) {
        var hexVersion = WindowsUtils.getWindowsVersionInFormat(minimumWindowsVersion, 'hex');
        if (hexVersion) {
            var versionDefs = [ 'WINVER', '_WIN32_WINNT', '_WIN32_WINDOWS' ];
            for (i in versionDefs) {
                args.push(input.cpp.defineFlag + versionDefs[i] + '=' + hexVersion);
            }
        }
    }

    if (product.cpp.debugInformation && product.cpp.separateDebugInformation)
        args.push("/Fd" + product.targetName + ".cl" + product.cpp.debugInfoSuffix);

    if (input.cpp.generateCompilerListingFiles)
        args.push("/Fa" + FileInfo.toWindowsSeparators(outputs.lst[0].filePath));

    if (input.cpp.enableCxxLanguageMacro && hasZCplusPlusOption(input))
        args.push("/Zc:__cplusplus");

    var objectMap = outputs.obj || outputs.intermediate_obj
    var objOutput = objectMap ? objectMap[0] : undefined
    args.push('/Fo' + FileInfo.toWindowsSeparators(objOutput.filePath))
    args.push(FileInfo.toWindowsSeparators(input.filePath))

    var preincludePaths = Cpp.collectPreincludePaths(input);
    args = args.concat([].uniqueConcat(preincludePaths).map(function(path) {
        return input.cpp.preincludeFlag + FileInfo.toWindowsSeparators(path);
    }));

    // Language
    if (tag === "cpp") {
        args.push("/TP");
        addCxxLanguageVersionFlag(input, args);
    } else if (tag === "c") {
        args.push("/TC");
        addCLanguageVersionFlag(input, args);
    }

    // Whether we're compiling a precompiled header or normal source file
    var pchOutput = outputs[tag + "_pch"] ? outputs[tag + "_pch"][0] : undefined;
    var pchInputs = explicitlyDependsOn[tag + "_pch"];
    if (pchOutput) {
        // create PCH
        if (input.qbs.toolchain.includes("clang-cl")) {
            // clang-cl does not support /Yc flag without filename
            args.push("/Yc" + FileInfo.toWindowsSeparators(input.filePath));
            // clang-cl complains when pch file is not included
            args.push("/FI" + FileInfo.toWindowsSeparators(input.filePath));
            args.push("/Fp" + FileInfo.toWindowsSeparators(pchOutput.filePath));
            args.push("/Fo" + FileInfo.toWindowsSeparators(objOutput.filePath));
        } else { // real msvc
            args.push("/Yc");
            args.push("/Fp" + FileInfo.toWindowsSeparators(pchOutput.filePath));
            args.push("/Fo" + FileInfo.toWindowsSeparators(objOutput.filePath));
            args.push(FileInfo.toWindowsSeparators(input.filePath));
        }

    } else if (pchInputs && pchInputs.length === 1
               && ModUtils.moduleProperty(input, "usePrecompiledHeader", tag)) {
        // use PCH
        var pchSourceArtifacts = product.artifacts[tag + "_pch_src"];
        if (pchSourceArtifacts && pchSourceArtifacts.length > 0) {
            var pchSourceFilePath = pchSourceArtifacts[0].filePath;
            var pchFilePath = FileInfo.toWindowsSeparators(pchInputs[0].filePath);
            args.push("/FI" + pchSourceFilePath);
            args.push("/Yu" + pchSourceFilePath);
            args.push("/Fp" + pchFilePath);
        } else {
            console.warning("products." + product.name + ".usePrecompiledHeader is true, "
                            + "but there is no " + tag + "_pch_src artifact.");
        }
    }

    args = args.concat(Cpp.collectMiscCompilerArguments(input, tag));

    var compilerPath = product.cpp.compilerPath;
    var wrapperArgs = product.cpp.compilerWrapper;
    if (wrapperArgs && wrapperArgs.length > 0) {
        args.unshift(compilerPath);
        compilerPath = wrapperArgs.shift();
        args = wrapperArgs.concat(args);
    }
    var cmd = new Command(compilerPath, args)
    cmd.description = (pchOutput ? 'pre' : '') + 'compiling ' + input.fileName;
    if (pchOutput)
        cmd.description += ' (' + tag + ')';
    cmd.highlight = "compiler";
    cmd.jobPool = "compiler";
    cmd.workingDirectory = product.buildDirectory;
    cmd.responseFileUsagePrefix = '@';
    // cl.exe outputs the cpp file name. We filter that out.
    cmd.inputFileName = input.fileName;
    cmd.relevantEnvironmentVariables = ["CL", "_CL_", "INCLUDE"];
    cmd.stdoutFilterFunction = function(output) {
        return output.split(inputFileName + "\r\n").join("");
    };
    return [cmd];
}

function linkerSupportsWholeArchive(product)
{
    return product.qbs.toolchainType.includes("clang-cl") ||
        Utilities.versionCompare(product.cpp.compilerVersion, "19.0.24215.1") >= 0
}

function handleDiscardProperty(product, flags) {
    var discardUnusedData = product.cpp.discardUnusedData;
    if (discardUnusedData === true)
        flags.push("/OPT:REF");
    else if (discardUnusedData === false)
        flags.push("/OPT:NOREF");
}

function prepareLinker(project, product, inputs, outputs, input, output) {
    var i;
    var linkDLL = (outputs.dynamiclibrary ? true : false)
    var primaryOutput = (linkDLL ? outputs.dynamiclibrary[0] : outputs.application[0])
    var debugInformation = product.cpp.debugInformation;
    var additionalManifestInputs = Array.prototype.map.call(inputs["native.pe.manifest"] || [],
        function (a) {
            return a.filePath;
        });
    var moduleDefinitionInputs = Array.prototype.map.call(inputs["def"] || [],
        function (a) {
            return a.filePath;
        });
    var generateManifestFiles = !linkDLL && product.cpp.generateManifestFile;
    var useClangCl = product.qbs.toolchain.includes("clang-cl");
    var canEmbedManifest = useClangCl || product.cpp.compilerVersionMajor >= 17 // VS 2012

    var linkerPath = effectiveLinkerPath(product, inputs);
    var useCompilerDriver = linkerPath === product.cpp.compilerPath;
    // args variable is built as follows:
    // [linkerWrapper] linkerPath /nologo [driverFlags driverLinkerFlags]
    //      allInputs libDeps [/link] linkerArgs
    var args = []

    if (useCompilerDriver) {
        args.push('/nologo');
        args = args.concat(Cpp.collectMiscDriverArguments(product),
                           Cpp.collectMiscLinkerArguments(product));
    }

    var allInputs = [].concat(Cpp.collectLinkerObjectPaths(inputs),
                              Cpp.collectResourceObjectPaths(inputs));
    args = args.concat([].uniqueConcat(allInputs).map(function(path) {
        return FileInfo.toWindowsSeparators(path);
    }));

    var linkerArgs = ['/nologo']
    if (linkDLL) {
        linkerArgs.push('/DLL');
        linkerArgs.push('/IMPLIB:' + FileInfo.toWindowsSeparators(outputs.dynamiclibrary_import[0].filePath));
    }

    if (debugInformation) {
        linkerArgs.push("/DEBUG");
        var debugInfo = outputs.debuginfo_app || outputs.debuginfo_dll;
        if (debugInfo)
            linkerArgs.push("/PDB:" + debugInfo[0].fileName);
    } else {
        linkerArgs.push('/INCREMENTAL:NO')
    }

    switch (product.qbs.architecture) {
    case "x86":
        linkerArgs.push("/MACHINE:X86");
        break;
    case "x86_64":
        linkerArgs.push("/MACHINE:X64");
        break;
    case "ia64":
        linkerArgs.push("/MACHINE:IA64");
        break;
    case "armv7":
        linkerArgs.push("/MACHINE:ARM");
        break;
    case "arm64":
        linkerArgs.push("/MACHINE:ARM64");
        break;
    }

    if (useCompilerDriver)
        handleClangClArchitectureFlags(product, product.qbs.architecture, args);

    var requireAppContainer = product.cpp.requireAppContainer;
    if (requireAppContainer !== undefined)
        linkerArgs.push("/APPCONTAINER" + (requireAppContainer ? "" : ":NO"));

    var minimumWindowsVersion = product.cpp.minimumWindowsVersion;
    var subsystemSwitch = undefined;
    if (minimumWindowsVersion || product.consoleApplication !== undefined) {
        // Ensure that we default to console if product.consoleApplication is undefined
        // since that could still be the case if only minimumWindowsVersion had been defined
        subsystemSwitch = product.consoleApplication === false ? '/SUBSYSTEM:WINDOWS' : '/SUBSYSTEM:CONSOLE';
    }

    var useLldLink = useCompilerDriver && product.cpp.linkerVariant === "lld"
            || !useCompilerDriver && product.cpp.linkerName === "lld-link.exe";
    if (minimumWindowsVersion) {
        var subsystemVersion = WindowsUtils.getWindowsVersionInFormat(minimumWindowsVersion,
                                                                      'subsystem');
        if (subsystemVersion) {
            subsystemSwitch += ',' + subsystemVersion;
            // llvm linker does not support /OSVERSION
            if (!useLldLink)
                linkerArgs.push('/OSVERSION:' + subsystemVersion);
        }
    }

    if (subsystemSwitch)
        linkerArgs.push(subsystemSwitch);

    var linkerOutputNativeFilePath = FileInfo.toWindowsSeparators(primaryOutput.filePath);
    var manifestFileNames = [];
    if (generateManifestFiles) {
        if (canEmbedManifest) {
            linkerArgs.push("/MANIFEST:embed");
            additionalManifestInputs.forEach(function (manifestFileName) {
                linkerArgs.push("/MANIFESTINPUT:" + manifestFileName);
            });
        } else {
            linkerOutputNativeFilePath
                    = FileInfo.toWindowsSeparators(
                        FileInfo.path(primaryOutput.filePath) + "/intermediate."
                            + primaryOutput.fileName);

            var manifestFileName = linkerOutputNativeFilePath + ".manifest";
            linkerArgs.push('/MANIFEST', '/MANIFESTFILE:' + manifestFileName);
            manifestFileNames = [manifestFileName].concat(additionalManifestInputs);
        }
    }

    if (moduleDefinitionInputs.length === 1)
        linkerArgs.push("/DEF:" + moduleDefinitionInputs[0]);
    else if (moduleDefinitionInputs.length > 1)
        throw new Error("Only one '.def' file can be specified for linking");

    var wholeArchiveSupported = linkerSupportsWholeArchive(product);
    var wholeArchiveRequested = false;
    var libDeps = Cpp.collectLibraryDependencies(product);
    var prevLib;
    for (i = 0; i < libDeps.length; ++i) {
        var dep = libDeps[i];
        var lib = dep.filePath;
        if (lib === prevLib)
            continue;
        prevLib = lib;

        if (wholeArchiveSupported && dep.wholeArchive) {
            // need to pass libraries to the driver to avoid "no input files" error if no object
            // files are specified; thus libraries are duplicated when using "WHOLEARCHIVE"
            if (useCompilerDriver && allInputs.length === 0)
                args.push(FileInfo.toWindowsSeparators(lib));
            linkerArgs.push("/WHOLEARCHIVE:" + FileInfo.toWindowsSeparators(lib));
        } else {
            args.push(FileInfo.toWindowsSeparators(lib));
        }
        if (dep.wholeArchive)
            wholeArchiveRequested = true;
    }
    if (wholeArchiveRequested && !wholeArchiveSupported) {
        console.warn("Product '" + product.name + "' sets cpp.linkWholeArchive on a dependency, "
                     + "but your linker does not support the /WHOLEARCHIVE option. "
                     + "Please upgrade to Visual Studio 2015 Update 2 or higher.");
    }

    if (product.cpp.entryPoint)
        linkerArgs.push("/ENTRY:" + product.cpp.entryPoint);

    if (outputs.application && product.cpp.generateLinkerMapFile) {
        if (useLldLink)
            linkerArgs.push("/lldmap:" + outputs.mem_map[0].filePath);
        else
            linkerArgs.push("/MAP:" + outputs.mem_map[0].filePath);
    }

    if (useCompilerDriver)
        args.push('/Fe' + linkerOutputNativeFilePath);
    else
        linkerArgs.push('/OUT:' + linkerOutputNativeFilePath);

    var libraryPaths = Cpp.collectLibraryPaths(product);
    linkerArgs = linkerArgs.concat([].uniqueConcat(libraryPaths).map(function(path) {
        return product.cpp.libraryPathFlag + FileInfo.toWindowsSeparators(path);
    }));

    handleDiscardProperty(product, linkerArgs);
    var linkerFlags = product.cpp.platformLinkerFlags.concat(product.cpp.linkerFlags);
    linkerArgs = linkerArgs.concat(linkerFlags);
    if (product.cpp.allowUnresolvedSymbols)
        linkerArgs.push("/FORCE:UNRESOLVED");

    var wrapperArgs = product.cpp.linkerWrapper;
    if (wrapperArgs && wrapperArgs.length > 0) {
        args.unshift(linkerPath);
        linkerPath = wrapperArgs.shift();
        args = wrapperArgs.concat(args);
    }
    var commands = [];
    var warningCmd = new JavaScriptCommand();
    warningCmd.silent = true;
    warningCmd.libDeps = libDeps;
    warningCmd.sourceCode = function() {
        for (var i = 0; i < libDeps.length; ++i) {
            if (!libDeps[i].productName || File.exists(libDeps[i].filePath))
                continue;
            console.warn("Import library '" + FileInfo.toNativeSeparators(libDeps[i].filePath)
                         + "' does not exist. This typically happens when a DLL does not "
                         + "export any symbols. Please make sure the '" + libDeps[i].productName
                         + "' library exports symbols, or, if you do not intend to actually "
                         + "link against it, specify \"cpp.link: false\" in the Depends item.");
        }
    };
    commands.push(warningCmd);

    if (linkerArgs.length !== 0)
        args = args.concat(useCompilerDriver ? ['/link'] : []).concat(linkerArgs);

    var cmd = new Command(linkerPath, args)
    cmd.description = 'linking ' + primaryOutput.fileName;
    cmd.highlight = 'linker';
    cmd.jobPool = "linker";
    cmd.relevantEnvironmentVariables = ["LINK", "_LINK_", "LIB", "TMP"];
    cmd.workingDirectory = FileInfo.path(primaryOutput.filePath)
    cmd.responseFileUsagePrefix = '@';
    cmd.responseFileSeparator = useCompilerDriver ? ' ' : '\n';
    cmd.stdoutFilterFunction = function(output) {
        res = output.replace(/^.*performing full link.*\s*/, "");
        res = res.replace(/^ *Creating library.*\s*/, "");
        res = res.replace(/^\s*Generating code\s*/, "");
        res = res.replace(/^\s*Finished generating code\s*/, "");
        return res;
    };
    commands.push(cmd);

    if (generateManifestFiles && !canEmbedManifest) {
        var outputNativeFilePath = FileInfo.toWindowsSeparators(primaryOutput.filePath);
        cmd = new JavaScriptCommand();
        cmd.src = linkerOutputNativeFilePath;
        cmd.dst = outputNativeFilePath;
        cmd.sourceCode = function() {
            File.copy(src, dst);
        }
        cmd.silent = true
        commands.push(cmd);

        args = ['/nologo', '/manifest'].concat(manifestFileNames);
        args.push("/outputresource:" + outputNativeFilePath + ";#" + (linkDLL ? "2" : "1"));
        cmd = new Command("mt.exe", args)
        cmd.description = 'embedding manifest into ' + primaryOutput.fileName;
        cmd.highlight = 'linker';
        cmd.workingDirectory = FileInfo.path(primaryOutput.filePath)
        commands.push(cmd);
    }

    if (product.cpp.shouldSignArtifacts) {
        Array.prototype.push.apply(
                    commands, Codesign.prepareSigntool(
                        project, product, inputs, outputs, input, output));
    }

    return commands;
}

function createRcCommand(binary, input, output, logo) {
    var platformDefines = input.cpp.platformDefines;
    var defines = input.cpp.defines;
    var includePaths = input.cpp.includePaths;
    var systemIncludePaths = input.cpp.systemIncludePaths;

    var args = [];
    if (logo === "can-suppress-logo")
        args.push("/nologo");
    for (i in platformDefines) {
        args.push("/d");
        args.push(platformDefines[i]);
    }
    for (i in defines) {
        args.push("/d");
        args.push(defines[i]);
    }
    for (i in includePaths) {
        args.push("/I");
        args.push(includePaths[i]);
    }
    for (i in systemIncludePaths) {
        args.push("/I");
        args.push(systemIncludePaths[i]);
    }
    args = args.concat(["/FO", output.filePath, input.filePath]);
    var cmd = new Command(binary, args);
    cmd.description = 'compiling ' + input.fileName;
    cmd.highlight = 'compiler';
    cmd.jobPool = "compiler";

    if (logo === "always-shows-logo") {
        // Remove the first two lines of stdout. That's the logo.
        cmd.stdoutFilterFunction = function(output) {
            var idx = 0;
            for (var i = 0; i < 3; ++i)
                idx = output.indexOf('\n', idx + 1);
            return output.substr(idx + 1);
        }
    }

    return cmd;
}
