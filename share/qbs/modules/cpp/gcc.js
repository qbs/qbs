/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
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

var File = loadExtension("qbs.File");
var FileInfo = loadExtension("qbs.FileInfo");
var DarwinTools = loadExtension("qbs.DarwinTools");
var ModUtils = loadExtension("qbs.ModUtils");
var PathTools = loadExtension("qbs.PathTools");
var Process = loadExtension("qbs.Process");
var UnixUtils = loadExtension("qbs.UnixUtils");
var WindowsUtils = loadExtension("qbs.WindowsUtils");

function escapeLinkerFlags(product, linkerFlags) {
    if (!linkerFlags || linkerFlags.length === 0)
        return [];

    var linkerName = product.moduleProperty("cpp", "linkerName") ;
    var needsEscape = linkerName === product.moduleProperty("cpp", "cxxCompilerName")
            || linkerName === product.moduleProperty("cpp", "cCompilerName");
    if (needsEscape) {
        var sep = ",";
        var useXlinker = linkerFlags.some(function (f) { return f.contains(sep); });
        if (useXlinker) {
            // One or more linker arguments contain the separator character itself
            // Use -Xlinker to handle these
            var xlinkerFlags = [];
            linkerFlags.map(function (linkerFlag) {
                xlinkerFlags.push("-Xlinker", linkerFlag);
            });
            return xlinkerFlags;
        }

        // If no linker arguments contain the separator character we can just use -Wl,
        // which is more compact and easier to read in logs
        return [["-Wl"].concat(linkerFlags).join(sep)];
    }

    return linkerFlags;
}

function linkerFlags(product, inputs, output) {
    var libraryPaths = ModUtils.moduleProperties(product, 'libraryPaths');
    var dynamicLibraries = ModUtils.moduleProperties(product, "dynamicLibraries");
    var staticLibraries = ModUtils.modulePropertiesFromArtifacts(product, inputs.staticlibrary, 'cpp', 'staticLibraries');
    var frameworks = ModUtils.moduleProperties(product, 'frameworks');
    var weakFrameworks = ModUtils.moduleProperties(product, 'weakFrameworks');
    var rpaths = (product.moduleProperty("cpp", "useRPaths") !== false)
            ? ModUtils.moduleProperties(product, 'rpaths') : undefined;
    var isDarwin = product.moduleProperty("qbs", "targetOS").contains("darwin");
    var i, args = additionalCompilerAndLinkerFlags(product);

    if (output.fileTags.contains("dynamiclibrary")) {
        args.push(isDarwin ? "-dynamiclib" : "-shared");

        if (isDarwin) {
            var internalVersion = product.moduleProperty("cpp", "internalVersion");
            if (internalVersion)
                args.push("-current_version", internalVersion);

            args = args.concat(escapeLinkerFlags(product, [
                                                     "-install_name",
                                                     UnixUtils.soname(product, output.fileName)]));
        } else {
            args = args.concat(escapeLinkerFlags(product, [
                                                     "-soname=" +
                                                     UnixUtils.soname(product, output.fileName)]));
        }
    }

    if (output.fileTags.contains("loadablemodule"))
        args.push(isDarwin ? "-bundle" : "-shared");

    if (output.fileTags.contains("dynamiclibrary") || output.fileTags.contains("loadablemodule")) {
        if (isDarwin)
            args = args.concat(escapeLinkerFlags(product, ["-headerpad_max_install_names"]));
        else
            args = args.concat(escapeLinkerFlags(product, ["--as-needed"]));
    }

    var arch = product.moduleProperty("cpp", "targetArch");
    if (isDarwin)
        args.push("-arch", arch);

    var minimumDarwinVersion = ModUtils.moduleProperty(product, "minimumDarwinVersion");
    if (minimumDarwinVersion) {
        var flag = ModUtils.moduleProperty(product, "minimumDarwinVersionLinkerFlag");
        if (flag)
            args = args.concat(escapeLinkerFlags(product, [flag, minimumDarwinVersion]));
    }

    var sysroot = ModUtils.moduleProperty(product, "sysroot");
    if (sysroot) {
        if (isDarwin)
            args = args.concat(escapeLinkerFlags(product, ["-syslibroot", sysroot]));
        else
            args.push("--sysroot=" + sysroot); // do not escape, compiler-as-linker also needs it
    }

    var unresolvedSymbolsAction = isDarwin ? "error" : "ignore-in-shared-libs";
    if (ModUtils.moduleProperty(product, "allowUnresolvedSymbols"))
        unresolvedSymbolsAction = isDarwin ? "suppress" : "ignore-all";
    args = args.concat(escapeLinkerFlags(product, isDarwin
                                         ? ["-undefined", unresolvedSymbolsAction]
                                         : ["--unresolved-symbols=" + unresolvedSymbolsAction]));

    for (i in rpaths)
        args = args.concat(escapeLinkerFlags(product, ["-rpath", rpaths[i]]));

    if (product.moduleProperty("qbs", "targetOS").contains('linux')) {
        var transitiveSOs = ModUtils.modulePropertiesFromArtifacts(product,
                                                                   inputs.dynamiclibrary_copy, 'cpp', 'transitiveSOs')
        var uniqueSOs = [].uniqueConcat(transitiveSOs)
        for (i in uniqueSOs) {
            // The real library is located one level up.
            args = args.concat(escapeLinkerFlags(product, [
                                                     "-rpath-link=" +
                                                     FileInfo.path(FileInfo.path(uniqueSOs[i]))]));
        }
    }

    if (product.moduleProperty("cpp", "entryPoint"))
        args = args.concat(escapeLinkerFlags(product, ["-e", product.moduleProperty("cpp", "entryPoint")]));

    if (product.moduleProperty("qbs", "toolchain").contains("mingw")) {
        if (product.consoleApplication !== undefined)
            args = args.concat(escapeLinkerFlags(product, [
                                                     "-subsystem",
                                                     product.consoleApplication
                                                        ? "console"
                                                        : "windows"]));

        var minimumWindowsVersion = ModUtils.moduleProperty(product, "minimumWindowsVersion");
        if (minimumWindowsVersion) {
            var subsystemVersion = WindowsUtils.getWindowsVersionInFormat(minimumWindowsVersion, 'subsystem');
            if (subsystemVersion) {
                var major = subsystemVersion.split('.')[0];
                var minor = subsystemVersion.split('.')[1];

                // http://sourceware.org/binutils/docs/ld/Options.html
                args = args.concat(escapeLinkerFlags(product, ["--major-subsystem-version", major]));
                args = args.concat(escapeLinkerFlags(product, ["--minor-subsystem-version", minor]));
                args = args.concat(escapeLinkerFlags(product, ["--major-os-version", major]));
                args = args.concat(escapeLinkerFlags(product, ["--minor-os-version", minor]));
            } else {
                console.warn('Unknown Windows version "' + minimumWindowsVersion + '"');
            }
        }
    }

    if (inputs.aggregate_infoplist)
        args.push("-sectcreate", "__TEXT", "__info_plist", inputs.aggregate_infoplist[0].filePath);

    var stdlib = product.moduleProperty("cpp", "cxxStandardLibrary");
    if (stdlib && product.moduleProperty("qbs", "toolchain").contains("clang"))
        args.push("-stdlib=" + stdlib);

    // Flags for library search paths
    if (libraryPaths)
        args = args.concat([].uniqueConcat(libraryPaths).map(function(path) { return '-L' + path }));

    var linkerScripts = inputs.linkerscript
            ? inputs.linkerscript.map(function(a) { return a.filePath; }) : [];
    args = args.concat([].uniqueConcat(linkerScripts).map(function(path) { return '-T' + path }));

    if (isDarwin && ModUtils.moduleProperty(product, "warningLevel") === "none")
        args.push('-w');

    args = args.concat(configFlags(product));
    args = args.concat(ModUtils.moduleProperties(product, 'platformLinkerFlags'));
    args = args.concat(ModUtils.moduleProperties(product, 'linkerFlags'));

    args.push("-o", output.filePath);

    if (inputs.obj)
        args = args.concat(inputs.obj.map(function (obj) { return obj.filePath }));

    // Add filenames of internal library dependencies to the lists
    var staticLibsFromInputs = inputs.staticlibrary
            ? inputs.staticlibrary.map(function(a) { return a.filePath; }) : [];
    staticLibraries = concatLibsFromArtifacts(staticLibraries, inputs.staticlibrary);
    var dynamicLibsFromInputs = inputs.dynamiclibrary_copy
            ? inputs.dynamiclibrary_copy.map(function(a) { return a.filePath; }) : [];
    dynamicLibraries = concatLibsFromArtifacts(dynamicLibraries, inputs.dynamiclibrary_copy);

    for (i in frameworks) {
        frameworkExecutablePath = PathTools.frameworkExecutablePath(frameworks[i]);
        if (File.exists(frameworkExecutablePath))
            args.push(frameworkExecutablePath);
        else
            args = args.concat(['-framework', frameworks[i]]);
    }

    for (i in weakFrameworks) {
        frameworkExecutablePath = PathTools.frameworkExecutablePath(weakFrameworks[i]);
        if (File.exists(frameworkExecutablePath))
            args = args.concat(['-weak_library', frameworkExecutablePath]);
        else
            args = args.concat(['-weak_framework', weakFrameworks[i]]);
    }

    for (i in staticLibraries) {
        if (staticLibsFromInputs.contains(staticLibraries[i]) || File.exists(staticLibraries[i])) {
            args.push(staticLibraries[i]);
        } else {
            args.push('-l' + staticLibraries[i]);
        }
    }

    for (i in dynamicLibraries) {
        if (dynamicLibsFromInputs.contains(dynamicLibraries[i])
                || File.exists(dynamicLibraries[i])) {
            args.push(dynamicLibraries[i]);
        } else {
            args.push('-l' + dynamicLibraries[i]);
        }
    }

    if (product.moduleProperty("qbs", "toolchain").contains("clang")
            && product.moduleProperty("qbs", "targetOS").contains("linux") && stdlib === "libc++")
        args.push("-lc++abi");

    return args;
}

// for compiler AND linker
function configFlags(config) {
    var args = [];

    var frameworkPaths = ModUtils.moduleProperties(config, 'frameworkPaths');
    if (frameworkPaths)
        args = args.concat(frameworkPaths.map(function(path) { return '-F' + path }));

    var systemFrameworkPaths = ModUtils.moduleProperties(config, 'systemFrameworkPaths');
    if (systemFrameworkPaths)
        args = args.concat(systemFrameworkPaths.map(function(path) { return '-iframework' + path }));

    return args;
}

function languageTagFromFileExtension(toolchain, fileName) {
    var i = fileName.lastIndexOf('.');
    if (i === -1)
        return;
    var m = {
        "c"     : "c",
        "C"     : "cpp",
        "cpp"   : "cpp",
        "cxx"   : "cpp",
        "c++"   : "cpp",
        "cc"    : "cpp",
        "m"     : "objc",
        "mm"    : "objcpp",
        "s"     : "asm",
        "S"     : "asm_cpp"
    };
    if (!toolchain.contains("clang"))
        m["sx"] = "asm_cpp"; // clang does NOT recognize .sx
    return m[fileName.substring(i + 1)];
}

function effectiveCompilerInfo(toolchain, input, output) {
    var compilerPath, language;
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(output.fileTags));

    // Whether we're compiling a precompiled header or normal source file
    var pchOutput = output.fileTags.contains(tag + "_pch");

    var compilerPathByLanguage = ModUtils.moduleProperty(input, "compilerPathByLanguage");
    if (compilerPathByLanguage)
        compilerPath = compilerPathByLanguage[tag];
    if (!compilerPath || tag !== languageTagFromFileExtension(toolchain, input.fileName))
        language = languageName(tag) + (pchOutput ? '-header' : '');
    if (!compilerPath)
        // fall back to main compiler
        compilerPath = ModUtils.moduleProperty(input, "compilerPath");
    return {
        path: compilerPath,
        language: language,
        tag: tag
    };
}

// ### what we actually need here is something like product.usedFileTags
//     that contains all fileTags that have been used when applying the rules.
function compilerFlags(product, input, output) {
    var includePaths = ModUtils.moduleProperties(input, 'includePaths');
    var systemIncludePaths = ModUtils.moduleProperties(input, 'systemIncludePaths');

    var platformDefines = ModUtils.moduleProperty(input, 'platformDefines');
    var defines = ModUtils.moduleProperties(input, 'defines');

    var EffectiveTypeEnum = { UNKNOWN: 0, LIB: 1, APP: 2 };
    var effectiveType = EffectiveTypeEnum.UNKNOWN;
    var libTypes = {staticlibrary : 1, dynamiclibrary : 1};
    var appTypes = {application : 1};
    var i;
    for (i = product.type.length; --i >= 0;) {
        if (libTypes.hasOwnProperty(product.type[i]) !== -1) {
            effectiveType = EffectiveTypeEnum.LIB;
            break;
        } else if (appTypes.hasOwnProperty(product.type[i]) !== -1) {
            effectiveType = EffectiveTypeEnum.APP;
            break;
        }
    }

    // Determine which C-language we're compiling
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(output.fileTags));
    if (!["c", "cpp", "objc", "objcpp", "asm_cpp"].contains(tag))
        throw ("unsupported source language: " + tag);

    var compilerInfo = effectiveCompilerInfo(product.moduleProperty("qbs", "toolchain"),
                                             input, output);

    var args = additionalCompilerAndLinkerFlags(product);

    if (haveTargetOption(product)) {
        args.push("-target", product.moduleProperty("cpp", "target"));
    } else {
        var arch = product.moduleProperty("cpp", "targetArch");
        if (product.moduleProperty("qbs", "targetOS").contains("darwin"))
            args.push("-arch", arch);

        if (arch === 'x86_64')
            args.push('-m64');
        else if (arch === 'i386')
            args.push('-m32');

        var minimumDarwinVersion = ModUtils.moduleProperty(product, "minimumDarwinVersion");
        if (minimumDarwinVersion) {
            var flag = ModUtils.moduleProperty(product, "minimumDarwinVersionCompilerFlag");
            if (flag)
                args.push(flag + "=" + minimumDarwinVersion);
        }
    }

    var sysroot = ModUtils.moduleProperty(product, "sysroot");
    if (sysroot) {
        if (product.moduleProperty("qbs", "targetOS").contains("darwin"))
            args.push("-isysroot", sysroot);
        else
            args.push("--sysroot=" + sysroot);
    }

    if (ModUtils.moduleProperty(input, "debugInformation"))
        args.push('-g');
    var opt = ModUtils.moduleProperty(input, "optimization")
    if (opt === 'fast')
        args.push('-O2');
    if (opt === 'small')
        args.push('-Os');
    if (opt === 'none')
        args.push('-O0');

    var warnings = ModUtils.moduleProperty(input, "warningLevel")
    if (warnings === 'none')
        args.push('-w');
    if (warnings === 'all') {
        args.push('-Wall');
        args.push('-Wextra');
    }
    if (ModUtils.moduleProperty(input, "treatWarningsAsErrors"))
        args.push('-Werror');

    args = args.concat(configFlags(input));
    args.push('-pipe');

    if (ModUtils.moduleProperty(input, "enableReproducibleBuilds")) {
        var toolchain = product.moduleProperty("qbs", "toolchain");
        if (!toolchain.contains("clang")) {
            var hashString = FileInfo.relativePath(project.sourceDirectory, input.filePath);
            var hash = qbs.getHash(hashString);
            args.push("-frandom-seed=0x" + hash.substring(0, 8));
        }

        var major = product.moduleProperty("cpp", "compilerVersionMajor");
        var minor = product.moduleProperty("cpp", "compilerVersionMinor");
        if ((toolchain.contains("clang") && (major > 3 || (major === 3 && minor >= 5))) ||
            (toolchain.contains("gcc") && (major > 4 || (major === 4 && minor >= 9)))) {
            args.push("-Wdate-time");
        }
    }

    var useArc = ModUtils.moduleProperty(input, "automaticReferenceCounting");
    if (useArc !== undefined && (tag === "objc" || tag === "objcpp")) {
        args.push(useArc ? "-fobjc-arc" : "-fno-objc-arc");
    }

    var visibility = ModUtils.moduleProperty(input, 'visibility');
    if (!product.type.contains('staticlibrary')
            && !product.moduleProperty("qbs", "toolchain").contains("mingw")) {
        if (visibility === 'hidden' || visibility === 'minimal')
            args.push('-fvisibility=hidden');
        if ((visibility === 'hiddenInlines' || visibility === 'minimal') && tag === 'cpp')
            args.push('-fvisibility-inlines-hidden');
        if (visibility === 'default')
            args.push('-fvisibility=default')
    }

    var prefixHeaders = ModUtils.moduleProperty(input, "prefixHeaders");
    for (i in prefixHeaders) {
        args.push('-include');
        args.push(prefixHeaders[i]);
    }

    if (compilerInfo.language)
        // Only push '-x language' if we have to.
        args.push("-x", compilerInfo.language);

    args = args.concat(ModUtils.moduleProperties(input, 'platformFlags'),
                       ModUtils.moduleProperties(input, 'flags'),
                       ModUtils.moduleProperties(input, 'platformFlags', tag),
                       ModUtils.moduleProperties(input, 'flags', tag));

    var pchOutput = output.fileTags.contains(compilerInfo.tag + "_pch");
    if (!pchOutput && ModUtils.moduleProperty(product, 'precompiledHeader', tag)) {
        var pchFilePath = FileInfo.joinPaths(
            ModUtils.moduleProperty(product, "precompiledHeaderDir"),
            product.name + "_" + tag);
        args.push('-include', pchFilePath);
    }

    var positionIndependentCode = input.moduleProperty('cpp', 'positionIndependentCode')
    if (effectiveType === EffectiveTypeEnum.LIB) {
        if (positionIndependentCode !== false && !product.moduleProperty("qbs", "toolchain").contains("mingw"))
            args.push('-fPIC');
    } else if (effectiveType === EffectiveTypeEnum.APP) {
        if (positionIndependentCode && !product.moduleProperty("qbs", "toolchain").contains("mingw"))
            args.push('-fPIE');
    } else {
        throw ("The product's type must be in " + JSON.stringify(
                   Object.getOwnPropertyNames(libTypes).concat(Object.getOwnPropertyNames(appTypes)))
                + ". But it is " + JSON.stringify(product.type) + '.');
    }
    var cppFlags = ModUtils.moduleProperties(input, 'cppFlags');
    for (i in cppFlags)
        args.push('-Wp,' + cppFlags[i])

    var allDefines = [];
    if (platformDefines)
        allDefines = allDefines.uniqueConcat(platformDefines);
    if (defines)
        allDefines = allDefines.uniqueConcat(defines);
    args = args.concat(allDefines.map(function(define) { return '-D' + define }));
    if (includePaths)
        args = args.concat([].uniqueConcat(includePaths).map(function(path) { return '-I' + path }));
    if (systemIncludePaths)
        args = args.concat([].uniqueConcat(systemIncludePaths).map(function(path) { return '-isystem' + path }));

    var minimumWindowsVersion = ModUtils.moduleProperty(input, "minimumWindowsVersion");
    if (minimumWindowsVersion && product.moduleProperty("qbs", "targetOS").contains("windows")) {
        var hexVersion = WindowsUtils.getWindowsVersionInFormat(minimumWindowsVersion, 'hex');
        if (hexVersion) {
            var versionDefs = [ 'WINVER', '_WIN32_WINNT', '_WIN32_WINDOWS' ];
            for (i in versionDefs)
                args.push('-D' + versionDefs[i] + '=' + hexVersion);
        } else {
            console.warn('Unknown Windows version "' + minimumWindowsVersion + '"');
        }
    }

    if (tag === "c" || tag === "objc") {
        var cVersion = ModUtils.moduleProperty(input, "cLanguageVersion");
        if (cVersion) {
            var gccCVersionsMap = {
                "c11": "c1x" // Deprecated, but compatible with older gcc versions.
            };
            args.push("-std=" + (gccCVersionsMap[cVersion] || cVersion));
        }
    }

    if (tag === "cpp" || tag === "objcpp") {
        var cxxVersion = ModUtils.moduleProperty(input, "cxxLanguageVersion");
        if (cxxVersion) {
            var gccCxxVersionsMap = {
                "c++11": "c++0x", // Deprecated, but compatible with older gcc versions.
                "c++14": "c++1y"
            };
            args.push("-std=" + (gccCxxVersionsMap[cxxVersion] || cxxVersion));
        }

        var cxxStandardLibrary = product.moduleProperty("cpp", "cxxStandardLibrary");
        if (cxxStandardLibrary && product.moduleProperty("qbs", "toolchain").contains("clang")) {
            args.push("-stdlib=" + cxxStandardLibrary);
        }
    }

    args.push("-o", output.filePath);
    args.push("-c", input.filePath);

    return args;
}

function haveTargetOption(product) {
    var toolchain = product.moduleProperty("qbs", "toolchain");
    var major = product.moduleProperty("cpp", "compilerVersionMajor");
    var minor = product.moduleProperty("cpp", "compilerVersionMinor");

    // Apple Clang 3.1 (shipped with Xcode 4.3) just happened to also correspond to LLVM 3.1,
    // so no special version check is needed for Apple
    return toolchain.contains("clang") && (major > 3 || (major === 3 && minor >= 1));
}

function additionalCompilerAndLinkerFlags(product) {
    var args = []

    var requireAppExtensionSafeApi = ModUtils.moduleProperty(product, "requireAppExtensionSafeApi");
    if (requireAppExtensionSafeApi !== undefined && product.moduleProperty("qbs", "targetOS").contains("darwin")) {
        args.push(requireAppExtensionSafeApi ? "-fapplication-extension" : "-fno-application-extension");
    }

    return args
}

// Returns the GCC language name equivalent to fileTag, accepted by the -x argument
function languageName(fileTag) {
    if (fileTag === 'c')
        return 'c';
    else if (fileTag === 'cpp')
        return 'c++';
    else if (fileTag === 'objc')
        return 'objective-c';
    else if (fileTag === 'objcpp')
        return 'objective-c++';
    else if (fileTag === 'asm')
        return 'assembler';
    else if (fileTag === 'asm_cpp')
        return 'assembler-with-cpp';
}

function prepareAssembler(project, product, inputs, outputs, input, output) {
    var assemblerPath = ModUtils.moduleProperty(product, "assemblerPath");

    var includePaths = ModUtils.moduleProperties(input, 'includePaths');
    var systemIncludePaths = ModUtils.moduleProperties(input, 'systemIncludePaths');

    var args = [];
    if (haveTargetOption(product)) {
        args.push("-target", product.moduleProperty("cpp", "target"));
    } else {
        var arch = product.moduleProperty("cpp", "targetArch");
        if (product.moduleProperty("qbs", "targetOS").contains("darwin"))
            args.push("-arch", arch);
        else if (arch === 'x86_64')
            args.push('--64');
        else if (arch === 'i386')
            args.push('--32');
    }

    if (ModUtils.moduleProperty(input, "debugInformation"))
        args.push('-g');

    var warnings = ModUtils.moduleProperty(input, "warningLevel")
    if (warnings === 'none')
        args.push('-W');

    var tag = "asm";
    if (tag !== languageTagFromFileExtension(product.moduleProperty("qbs", "toolchain"),
                                             input.fileName))
        // Only push '-x language' if we have to.
        args.push("-x", languageName(tag));

    args = args.concat(ModUtils.moduleProperties(input, 'platformFlags', tag),
                       ModUtils.moduleProperties(input, 'flags', tag));

    var allIncludePaths = [];
    if (systemIncludePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(systemIncludePaths);
    if (includePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(includePaths);
    args = args.concat(allIncludePaths.map(function(path) { return '-I' + path }));

    args.push("-o", output.filePath);
    args.push(input.filePath);

    var cmd = new Command(assemblerPath, args);
    cmd.description = "assembling " + input.fileName;
    cmd.highlight = "compiler";
    return cmd;
}

function prepareCompiler(project, product, inputs, outputs, input, output) {
    var compilerInfo = effectiveCompilerInfo(product.moduleProperty("qbs", "toolchain"),
                                             input, output);
    var compilerPath = compilerInfo.path;
    var pchOutput = output.fileTags.contains(compilerInfo.tag + "_pch");

    var args = compilerFlags(product, input, output);
    var wrapperArgsLength = 0;
    var wrapperArgs = ModUtils.moduleProperty(product, "compilerWrapper");
    if (wrapperArgs && wrapperArgs.length > 0) {
        wrapperArgsLength = wrapperArgs.length;
        args.unshift(compilerPath);
        compilerPath = wrapperArgs.shift();
        args = wrapperArgs.concat(args);
    }

    var cmd = new Command(compilerPath, args);
    cmd.description = (pchOutput ? 'pre' : '') + 'compiling ' + input.fileName;
    if (pchOutput)
        cmd.description += ' (' + compilerInfo.tag + ')';
    cmd.highlight = "compiler";
    cmd.responseFileArgumentIndex = wrapperArgsLength;
    cmd.responseFileUsagePrefix = '@';
    return cmd;
}

// Concatenates two arrays of library names and preserves the dependency order that ld needs.
function concatLibs(libs, deplibs) {
    var r = [];
    var s = {};

    function addLibs(lst) {
        for (var i = lst.length; --i >= 0;) {
            var lib = lst[i];
            if (!s[lib]) {
                s[lib] = true;
                r.unshift(lib);
            }
        }
    }

    addLibs(deplibs);
    addLibs(libs);
    return r;
}

function collectTransitiveSos(inputs)
{
    var result = [];
    for (var i in inputs.dynamiclibrary_copy) {
        var lib = inputs.dynamiclibrary_copy[i];
        var impliedLibs = ModUtils.moduleProperties(lib, 'transitiveSOs');
        var libsToAdd = [lib.filePath].concat(impliedLibs);
        result = result.concat(libsToAdd);
    }
    result = concatLibs([], result);
    return result;
}

function prepareLinker(project, product, inputs, outputs, input, output) {
    var i, primaryOutput, cmd, commands = [];

    if (outputs.application) {
        primaryOutput = outputs.application[0];
    } else if (outputs.dynamiclibrary) {
        primaryOutput = outputs.dynamiclibrary[0];
    } else if (outputs.loadablemodule) {
        primaryOutput = outputs.loadablemodule[0];
    }

    cmd = new Command(ModUtils.moduleProperty(product, "linkerPath"),
                      linkerFlags(product, inputs, primaryOutput));
    cmd.description = 'linking ' + primaryOutput.fileName;
    cmd.highlight = 'linker';
    cmd.responseFileUsagePrefix = '@';
    commands.push(cmd);

    if (outputs.debuginfo) {
        if (product.moduleProperty("qbs", "targetOS").contains("darwin")) {
            var dsymPath = outputs.debuginfo[0].filePath;
            if (outputs.debuginfo_bundle && outputs.debuginfo_bundle[0])
                dsymPath = outputs.debuginfo_bundle[0].filePath;
            var flags = ModUtils.moduleProperty(product, "dsymutilFlags") || [];
            cmd = new Command(ModUtils.moduleProperty(product, "dsymutilPath"), flags.concat([
                "-o", dsymPath, primaryOutput.filePath
            ]));
            cmd.description = "generating dSYM for " + product.name;
            commands.push(cmd);
        } else {
            cmd = new Command(ModUtils.moduleProperty(product, "objcopyPath"), [
                                  "--only-keep-debug", primaryOutput.filePath,
                                  outputs.debuginfo[0].filePath
                              ]);
            cmd.silent = true;
            commands.push(cmd);

            cmd = new Command(ModUtils.moduleProperty(product, "stripPath"), [
                                  "--strip-debug", primaryOutput.filePath
                              ]);
            cmd.silent = true;
            commands.push(cmd);

            cmd = new Command(ModUtils.moduleProperty(product, "objcopyPath"), [
                                  "--add-gnu-debuglink=" + outputs.debuginfo[0].filePath,
                                  primaryOutput.filePath
                              ]);
            cmd.silent = true;
            commands.push(cmd);
        }
    }

    if (outputs.dynamiclibrary) {
        // Update the copy, if any global symbols have changed.
        cmd = new JavaScriptCommand();
        cmd.silent = true;
        cmd.sourceCode = function() {
            var sourceFilePath = outputs.dynamiclibrary[0].filePath;
            var targetFilePath = outputs.dynamiclibrary_copy[0].filePath;
            if (!File.exists(targetFilePath)) {
                File.copy(sourceFilePath, targetFilePath);
                return;
            }
            if (product.moduleProperty("qbs", "toolchain").contains("mingw")) {
                // mingw's nm tool does not work correctly.
                File.copy(sourceFilePath, targetFilePath);
                return;
            }
            var process = new Process();
            var command = ModUtils.moduleProperty(product, "nmPath");
            var args = ["-g", "-D", "-P"];
            if (process.exec(command, args.concat(sourceFilePath), false) !== 0) {
                // Failure to run the nm tool is not fatal. We just fall back to the
                // "always relink" behavior.
                File.copy(sourceFilePath, targetFilePath);
                return;
            }
            var globalSymbolsSource = process.readStdOut();
            if (process.exec(command, args.concat(targetFilePath), false) !== 0) {
                File.copy(sourceFilePath, targetFilePath);
                return;
            }
            var globalSymbolsTarget = process.readStdOut();

            var globalSymbolsSourceLines = globalSymbolsSource.split('\n');
            var globalSymbolsTargetLines = globalSymbolsTarget.split('\n');
            if (globalSymbolsSourceLines.length !== globalSymbolsTargetLines.length) {
                var checkMode = ModUtils.moduleProperty(product, "exportedSymbolsCheckMode");
                if (checkMode === "strict") {
                    File.copy(sourceFilePath, targetFilePath);
                    return;
                }

                // Collect undefined symbols and remove them from the symbol lists.
                // GNU nm has the "--defined" option for this purpose, but POSIX nm does not.
                args.push("-u");
                if (process.exec(command, args.concat(sourceFilePath), false) !== 0) {
                    File.copy(sourceFilePath, targetFilePath);
                    return;
                }
                var undefinedSymbolsSource = process.readStdOut();
                if (process.exec(command, args.concat(targetFilePath), false) !== 0) {
                    File.copy(sourceFilePath, targetFilePath);
                    return;
                }
                var undefinedSymbolsTarget = process.readStdOut();
                process.close();

                var undefinedSymbolsSourceLines = undefinedSymbolsSource.split('\n');
                var undefinedSymbolsTargetLines = undefinedSymbolsTarget.split('\n');

                globalSymbolsSourceLines = globalSymbolsSourceLines.filter(function(line) {
                    return !undefinedSymbolsSourceLines.contains(line); });
                globalSymbolsTargetLines = globalSymbolsTargetLines.filter(function(line) {
                    return !undefinedSymbolsTargetLines.contains(line); });
                if (globalSymbolsSourceLines.length !== globalSymbolsTargetLines.length) {
                    File.copy(sourceFilePath, targetFilePath);
                    return;
                }
            }

            while (globalSymbolsSourceLines.length > 0) {
                var sourceLine = globalSymbolsSourceLines.shift();
                var targetLine = globalSymbolsTargetLines.shift();
                var sourceLineElems = sourceLine.split(/\s+/);
                var targetLineElems = targetLine.split(/\s+/);
                if (sourceLineElems[0] !== targetLineElems[0] // Object name.
                        || sourceLineElems[1] !== targetLineElems[1]) { // Object type
                    File.copy(sourceFilePath, targetFilePath);
                    return;
                }
            }
        }
        commands.push(cmd);

        // Create symlinks from {libfoo, libfoo.1, libfoo.1.0} to libfoo.1.0.0
        var links = outputs["dynamiclibrary_symlink"];
        var symlinkCount = links ? links.length : 0;
        for (i = 0; i < symlinkCount; ++i) {
            cmd = new Command("ln", ["-sf", primaryOutput.fileName,
                                     links[i].filePath]);
            cmd.highlight = "filegen";
            cmd.description = "creating symbolic link '"
                    + links[i].fileName + "'";
            cmd.workingDirectory = FileInfo.path(primaryOutput.filePath);
            commands.push(cmd);
        }
    }

    var actualSigningIdentity = product.moduleProperty("xcode", "actualSigningIdentity");
    var codesignDisplayName = product.moduleProperty("xcode", "actualSigningIdentityDisplayName");
    if (actualSigningIdentity && !product.moduleProperty("bundle", "isBundle")) {
        var args = product.moduleProperty("xcode", "codesignFlags") || [];
        args.push("--force");
        args.push("--sign", actualSigningIdentity);
        args = args.concat(DarwinTools._codeSignTimestampFlags(product));

        for (var j in inputs.xcent) {
            args.push("--entitlements", inputs.xcent[j].filePath);
            break; // there should only be one
        }
        args.push(primaryOutput.filePath);
        cmd = new Command(product.moduleProperty("xcode", "codesignPath"), args);
        cmd.description = "codesign "
                + primaryOutput.fileName
                + " using " + codesignDisplayName
                + " (" + actualSigningIdentity + ")";
        commands.push(cmd);
    }

    return commands;
}

function concatLibsFromArtifacts(libs, artifacts)
{
    if (!artifacts)
        return libs;
    var deps = artifacts.map(function (a) { return a.filePath; });
    deps.reverse();
    return concatLibs(deps, libs);
}
