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
var ModUtils = loadExtension("qbs.ModUtils");
var PathTools = loadExtension("qbs.PathTools");
var UnixUtils = loadExtension("qbs.UnixUtils");
var WindowsUtils = loadExtension("qbs.WindowsUtils");

function linkerFlags(product, inputs) {
    var libraryPaths = ModUtils.moduleProperties(product, 'libraryPaths');
    var dynamicLibraries = ModUtils.moduleProperties(product, "dynamicLibraries");
    var staticLibraries = ModUtils.modulePropertiesFromArtifacts(product, inputs.staticlibrary, 'cpp', 'staticLibraries');
    var linkerScripts = ModUtils.moduleProperties(product, 'linkerScripts');
    var frameworks = ModUtils.moduleProperties(product, 'frameworks');
    var weakFrameworks = ModUtils.moduleProperties(product, 'weakFrameworks');
    var rpaths = (product.moduleProperty("cpp", "useRPaths") !== false)
            ? ModUtils.moduleProperties(product, 'rpaths') : undefined;
    var args = [], i;

    if (rpaths && rpaths.length)
        args.push('-Wl,-rpath,' + rpaths.join(",-rpath,"));

    // Add filenames of internal library dependencies to the lists
    var staticLibsFromInputs = inputs.staticlibrary
            ? inputs.staticlibrary.map(function(a) { return a.filePath; }) : [];
    staticLibraries = concatLibsFromArtifacts(staticLibraries, inputs.staticlibrary);
    var dynamicLibsFromInputs = inputs.dynamiclibrary_copy
            ? inputs.dynamiclibrary_copy.map(function(a) { return a.filePath; }) : [];
    dynamicLibraries = concatLibsFromArtifacts(dynamicLibraries, inputs.dynamiclibrary_copy);

    // Flags for library search paths
    if (libraryPaths)
        args = args.concat(libraryPaths.map(function(path) { return '-L' + path }));

    if (linkerScripts)
        args = args.concat(linkerScripts.map(function(path) { return '-T' + path }));

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

    var isDarwin = product.moduleProperty("qbs", "targetOS").contains("darwin");
    var unresolvedSymbolsAction = isDarwin ? "error" : "ignore-in-shared-libs";
    if (ModUtils.moduleProperty(product, "allowUnresolvedSymbols"))
        unresolvedSymbolsAction = isDarwin ? "suppress" : "ignore-all";
    var unresolvedSymbolsKey = isDarwin ? "-undefined," : "--unresolved-symbols=";
    args.push("-Wl," + unresolvedSymbolsKey + unresolvedSymbolsAction);

    if (product.moduleProperty("qbs", "targetOS").contains('linux')) {
        var transitiveSOs = ModUtils.modulePropertiesFromArtifacts(product,
                                                                   inputs.dynamiclibrary_copy, 'cpp', 'transitiveSOs')
        var uniqueSOs = [].uniqueConcat(transitiveSOs)
        for (i in uniqueSOs) {
            // The real library is located one level up.
            args.push("-Wl,-rpath-link=" + FileInfo.path(FileInfo.path(uniqueSOs[i])));
        }
    }

    if (product.moduleProperty("qbs", "toolchain").contains("clang")) {
        var stdlib = product.moduleProperty("cpp", "cxxStandardLibrary");
        if (stdlib) {
            args.push("-stdlib=" + stdlib);
        }

        if (product.moduleProperty("qbs", "targetOS").contains("linux") && stdlib === "libc++")
            args.push("-lc++abi");
    }

    return args;
}

// for compiler AND linker
function configFlags(config) {
    var args = [];

    var arch = ModUtils.moduleProperty(config, "architecture")
    if (config.moduleProperty("qbs", "toolchain").contains("llvm") &&
        config.moduleProperty("qbs", "targetOS").contains("darwin")) {
        args.push("-arch");
        args.push(arch === "x86" ? "i386" : arch);
    } else {
        if (arch === 'x86_64')
            args.push('-m64');
        else if (arch === 'x86')
            args.push('-m32');
    }

    if (ModUtils.moduleProperty(config, "debugInformation"))
        args.push('-g');
    var opt = ModUtils.moduleProperty(config, "optimization")
    if (opt === 'fast')
        args.push('-O2');
    if (opt === 'small')
        args.push('-Os');
    if (opt === 'none')
        args.push('-O0');

    var warnings = ModUtils.moduleProperty(config, "warningLevel")
    if (warnings === 'none')
        args.push('-w');
    if (warnings === 'all') {
        args.push('-Wall');
        args.push('-Wextra');
    }
    if (ModUtils.moduleProperty(config, "treatWarningsAsErrors"))
        args.push('-Werror');

    var frameworkPaths = ModUtils.moduleProperties(config, 'frameworkPaths');
    if (frameworkPaths)
        args = args.concat(frameworkPaths.map(function(path) { return '-F' + path }));

    var systemFrameworkPaths = ModUtils.moduleProperties(config, 'systemFrameworkPaths');
    if (systemFrameworkPaths)
        args = args.concat(systemFrameworkPaths.map(function(path) { return '-iframework' + path }));

    return args;
}

// ### what we actually need here is something like product.usedFileTags
//     that contains all fileTags that have been used when applying the rules.
function additionalCompilerFlags(product, input, output) {
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

    var args = []
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

    if (platformDefines)
        args = args.concat(platformDefines.map(function(define) { return '-D' + define }));
    if (defines)
        args = args.concat(defines.map(function(define) { return '-D' + define }));
    if (includePaths)
        args = args.concat(includePaths.map(function(path) { return '-I' + path }));
    if (systemIncludePaths)
        args = args.concat(systemIncludePaths.map(function(path) { return '-isystem' + path }));

    var minimumWindowsVersion = ModUtils.moduleProperty(input, "minimumWindowsVersion");
    if (minimumWindowsVersion && product.moduleProperty("qbs", "targetOS").contains("windows")) {
        var hexVersion = WindowsUtils.getWindowsVersionInFormat(minimumWindowsVersion, 'hex');
        if (hexVersion) {
            var versionDefs = [ 'WINVER', '_WIN32_WINNT', '_WIN32_WINDOWS' ];
            for (i in versionDefs)
                args.push('-D' + versionDefs[i] + '=' + hexVersion);
        } else {
            print('WARNING: Unknown Windows version "' + minimumWindowsVersion + '"');
        }
    }

    args.push('-c');
    args.push(input.filePath);
    args.push('-o');
    args.push(output.filePath);
    return args
}

function additionalCompilerAndLinkerFlags(product) {
    var args = []

    var sysroot = ModUtils.moduleProperty(product, "sysroot");
    if (sysroot) {
        if (product.moduleProperty("qbs", "targetOS").contains("darwin"))
            args.push("-isysroot", sysroot);
        else
            args.push("--sysroot=" + sysroot);
    }

    var minimumOsxVersion = ModUtils.moduleProperty(product, "minimumOsxVersion");
    if (minimumOsxVersion && product.moduleProperty("qbs", "targetOS").contains("osx"))
        args.push('-mmacosx-version-min=' + minimumOsxVersion);

    var minimumiOSVersion = ModUtils.moduleProperty(product, "minimumIosVersion");
    if (minimumiOSVersion && product.moduleProperty("qbs", "targetOS").contains("ios")) {
        if (product.moduleProperty("qbs", "targetOS").contains("ios-simulator"))
            args.push('-mios-simulator-version-min=' + minimumiOSVersion);
        else
            args.push('-miphoneos-version-min=' + minimumiOSVersion);
    }

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

function prepareCompiler(project, product, inputs, outputs, input, output) {

    function languageTagFromFileExtension(fileName) {
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
            "S"     : "asm_cpp",
            "sx"    : "asm_cpp"
        };
        return m[fileName.substring(i + 1)];
    }

    var i, c;

    // Determine which C-language we're compiling
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(output.fileTags));
    if (!["c", "cpp", "objc", "objcpp", "asm", "asm_cpp"].contains(tag))
        throw ("unsupported source language: " + tag);

    // Whether we're compiling a precompiled header or normal source file
    var pchOutput = outputs[tag + "_pch"] ? outputs[tag + "_pch"][0] : undefined;

    var args = configFlags(input);
    args.push('-pipe');

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

    var compilerPath;
    var compilerPathByLanguage = ModUtils.moduleProperty(input, "compilerPathByLanguage");
    if (compilerPathByLanguage)
        compilerPath = compilerPathByLanguage[tag];
    if (!compilerPath || tag !== languageTagFromFileExtension(input.fileName)) {
        // Only push '-x language' if we have to.
        args.push('-x');
        args.push(languageName(tag) + (pchOutput ? '-header' : ''));
    }
    if (!compilerPath) {
        // fall back to main compiler
        compilerPath = ModUtils.moduleProperty(input, "compilerPath");
    }

    args = args.concat(ModUtils.moduleProperties(input, 'platformFlags'),
                       ModUtils.moduleProperties(input, 'flags'),
                       ModUtils.moduleProperties(input, 'platformFlags', tag),
                       ModUtils.moduleProperties(input, 'flags', tag));

    if (!pchOutput && ModUtils.moduleProperty(product, 'precompiledHeader', tag)) {
        var pchFilePath = FileInfo.joinPaths(
            ModUtils.moduleProperty(product, "precompiledHeaderDir"),
            product.name + "_" + tag);
        args.push('-include', pchFilePath);
    }

    args = args.concat(additionalCompilerFlags(product, input, output));
    args = args.concat(additionalCompilerAndLinkerFlags(product));

    var wrapperArgs = ModUtils.moduleProperty(product, "compilerWrapper");
    if (wrapperArgs && wrapperArgs.length > 0) {
        args.unshift(compilerPath);
        compilerPath = wrapperArgs.shift();
        args = wrapperArgs.concat(args);
    }

    if (tag === "c" || tag === "objc") {
        var cVersion = ModUtils.moduleProperty(input, "cLanguageVersion");
        if (cVersion) {
            var gccCVersionsMap = {
                "c89": "c89",
                "c99": "c99",
                "c11": "c1x" // Deprecated, but compatible with older gcc versions.
            };
            args.push("-std=" + gccCVersionsMap[cVersion]);
        }
    }

    if (tag === "cpp" || tag === "objcpp") {
        var cxxVersion = ModUtils.moduleProperty(input, "cxxLanguageVersion");
        if (cxxVersion) {
            var gccCxxVersionsMap = {
                "c++98": "c++98",
                "c++11": "c++0x", // Deprecated, but compatible with older gcc versions.
                "c++14": "c++1y"
            };
            args.push("-std=" + gccCxxVersionsMap[cxxVersion]);
        }

        var cxxStandardLibrary = product.moduleProperty("cpp", "cxxStandardLibrary");
        if (cxxStandardLibrary && product.moduleProperty("qbs", "toolchain").contains("clang")) {
            args.push("-stdlib=" + cxxStandardLibrary);
        }
    }

    var cmd = new Command(compilerPath, args);
    cmd.description = (pchOutput ? 'pre' : '') + 'compiling ' + input.fileName;
    if (pchOutput)
        cmd.description += ' (' + tag + ')';
    cmd.highlight = "compiler";
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
    var i, primaryOutput, cmd, commands = [], args = [];

    if (outputs.application) {
        primaryOutput = outputs.application[0];
    } else if (outputs.dynamiclibrary) {
        primaryOutput = outputs.dynamiclibrary[0];

        args.push("-shared");

        if (product.moduleProperty("qbs", "targetOS").contains("linux")) {
            args.push("-Wl,--as-needed");
            args.push("-Wl,-soname=" + UnixUtils.soname(product, primaryOutput.fileName));
        } else if (product.moduleProperty("qbs", "targetOS").contains("darwin")) {
            args.push("-Wl,-install_name," + UnixUtils.soname(product, primaryOutput.fileName));
            args.push("-Wl,-headerpad_max_install_names");

            var internalVersion = product.moduleProperty("cpp", "internalVersion");
            if (internalVersion)
                args.push("-current_version", internalVersion);
        }
    } else if (outputs.loadablemodule) {
        primaryOutput = outputs.loadablemodule[0];

        args.push("-bundle");

        if (product.moduleProperty("qbs", "targetOS").contains("darwin")) {
            args.push("-Wl,-headerpad_max_install_names");
        }
    }

    if (product.moduleProperty("cpp", "entryPoint"))
        args.push("-Wl,-e," + product.moduleProperty("cpp", "entryPoint"));

    if (product.moduleProperty("qbs", "toolchain").contains("mingw")) {
        if (product.consoleApplication !== undefined)
            if (product.consoleApplication)
                args.push("-Wl,-subsystem,console");
            else
                args.push("-Wl,-subsystem,windows");

        var minimumWindowsVersion = ModUtils.moduleProperty(product, "minimumWindowsVersion");
        if (minimumWindowsVersion) {
            var subsystemVersion = WindowsUtils.getWindowsVersionInFormat(minimumWindowsVersion, 'subsystem');
            if (subsystemVersion) {
                var major = subsystemVersion.split('.')[0];
                var minor = subsystemVersion.split('.')[1];

                // http://sourceware.org/binutils/docs/ld/Options.html
                args.push("-Wl,--major-subsystem-version," + major);
                args.push("-Wl,--minor-subsystem-version," + minor);
                args.push("-Wl,--major-os-version," + major);
                args.push("-Wl,--minor-os-version," + minor);
            } else {
                print('WARNING: Unknown Windows version "' + minimumWindowsVersion + '"');
            }
        }
    }

    if (inputs.infoplist)
        args.push("-sectcreate", "__TEXT", "__info_plist", inputs.infoplist[0].filePath);

    if (inputs.obj)
        args = args.concat(inputs.obj.map(function (obj) { return obj.filePath }));

    args = args.concat(configFlags(product));
    args = args.concat(linkerFlags(product, inputs));
    args = args.concat(additionalCompilerAndLinkerFlags(product));

    args = args.concat(ModUtils.moduleProperties(product, 'platformLinkerFlags'));
    args = args.concat(ModUtils.moduleProperties(product, 'linkerFlags'));

    args.push("-o");
    args.push(primaryOutput.filePath);

    cmd = new Command(ModUtils.moduleProperty(product, "linkerPath"), args);
    cmd.description = 'linking ' + primaryOutput.fileName;
    cmd.highlight = 'linker';
    cmd.responseFileUsagePrefix = '@';
    commands.push(cmd);

    if (outputs.debuginfo) {
        if (product.moduleProperty("qbs", "targetOS").contains("darwin")) {
            var flags = ModUtils.moduleProperty(product, "dsymutilFlags") || [];
            cmd = new Command(ModUtils.moduleProperty(product, "dsymutilPath"), flags.concat([
                "-o", outputs.debuginfo[0].filePath,
                primaryOutput.filePath
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
