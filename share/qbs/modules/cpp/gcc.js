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
var Host = require("qbs.Host");
var DarwinTools = require("qbs.DarwinTools");
var ModUtils = require("qbs.ModUtils");
var PathTools = require("qbs.PathTools");
var Process = require("qbs.Process");
var TextFile = require("qbs.TextFile");
var UnixUtils = require("qbs.UnixUtils");
var Utilities = require("qbs.Utilities");
var WindowsUtils = require("qbs.WindowsUtils");

function effectiveLinkerPath(product, inputs) {
    if (product.cpp.linkerMode === "automatic") {
        var compilers = product.cpp.compilerPathByLanguage;
        if (compilers) {
            if (inputs.cpp_obj || inputs.cpp_staticlibrary) {
                console.log("Found C++ or Objective-C++ objects, choosing C++ linker for "
                            + product.name);
                return compilers["cpp"];
            }

            if (inputs.c_obj || inputs.c_staticlibrary) {
                console.log("Found C or Objective-C objects, choosing C linker for "
                            + product.name);
                return compilers["c"];
            }
        }

        console.log("Found no C-language objects, choosing system linker for "
                    + product.name);
    }

    return product.cpp.linkerPath;
}

function useCompilerDriverLinker(product, inputs) {
    var linker = effectiveLinkerPath(product, inputs);
    var compilers = product.cpp.compilerPathByLanguage;
    if (compilers) {
        return linker === compilers["cpp"]
            || linker === compilers["c"];
    }
    return linker === product.cpp.compilerPath;
}

function collectLibraryDependencies(product, isDarwin) {
    var publicDeps = {};
    var privateDeps = {};
    var objects = [];
    var objectByFilePath = {};
    var tagForLinkingAgainstSharedLib = product.cpp.imageFormat === "pe"
            ? "dynamiclibrary_import" : "dynamiclibrary";
    var removeDuplicateLibraries = product.cpp.removeDuplicateLibraries

    function addObject(obj, addFunc) {
        /* If the object is already known, remove its previous usage and insert
         * it again in the new desired position. This preserves the order of
         * the other objects, and is analogous to what qmake does (see the
         * mergeLflags parameter in UnixMakefileGenerator::findLibraries()).
         */
        if (removeDuplicateLibraries && (obj.filePath in objectByFilePath)) {
            var oldObj = objectByFilePath[obj.filePath];
            var i = objects.indexOf(oldObj);
            if (i >= 0)
                objects.splice(i, 1);
        }
        addFunc.call(objects, obj);
        objectByFilePath[obj.filePath] = obj;
    }

    function addPublicFilePath(filePath, dep) {
        var existing = objectByFilePath[filePath];
        var wholeArchive = dep.parameters.cpp && dep.parameters.cpp.linkWholeArchive;
        var symbolLinkMode = dep.parameters.cpp && dep.parameters.cpp.symbolLinkMode;
        if (existing) {
            existing.direct = true;
            existing.wholeArchive = wholeArchive;
            existing.symbolLinkMode = symbolLinkMode;
        } else {
            addObject({ direct: true, filePath: filePath,
                        wholeArchive: wholeArchive, symbolLinkMode: symbolLinkMode },
                      Array.prototype.unshift);
        }
    }

    function addPrivateFilePath(filePath) {
        var existing = objectByFilePath[filePath];
        if (!existing)
            addObject({ direct: false, filePath: filePath }, Array.prototype.unshift);
    }

    function addArtifactFilePaths(dep, tag, addFunction) {
        var artifacts = dep.artifacts[tag];
        if (!artifacts)
            return;
        var artifactFilePaths = artifacts.map(function(a) { return a.filePath; });
        for (var i = 0; i < artifactFilePaths.length; ++i)
           addFunction(artifactFilePaths[i], dep);
    }

    function addExternalLibs(obj) {
        if (!obj.cpp)
            return;
        function ensureArray(a) {
            return (a instanceof Array) ? a : [];
        }
        function sanitizedModuleListProperty(obj, moduleName, propertyName) {
            return ensureArray(ModUtils.sanitizedModuleProperty(obj, moduleName, propertyName));
        }
        var externalLibs = [].concat(
                    ensureArray(sanitizedModuleListProperty(obj, "cpp", "staticLibraries")),
                    ensureArray(sanitizedModuleListProperty(obj, "cpp", "dynamicLibraries")));
        for (var i = 0, len = externalLibs.length; i < len; ++i)
            addObject({ direct: true, filePath: externalLibs[i] }, Array.prototype.push);
        if (isDarwin) {
            externalLibs = [].concat(
                        ensureArray(sanitizedModuleListProperty(obj, "cpp", "frameworks")));
            for (var i = 0, len = externalLibs.length; i < len; ++i)
                addObject({ direct: true, filePath: externalLibs[i], framework: true },
                        Array.prototype.push);
            externalLibs = [].concat(
                        ensureArray(sanitizedModuleListProperty(obj, "cpp", "weakFrameworks")));
            for (var i = 0, len = externalLibs.length; i < len; ++i)
                addObject({ direct: true, filePath: externalLibs[i], framework: true,
                            symbolLinkMode: "weak" }, Array.prototype.push);
        }
    }

    function traverse(dep, isBelowIndirectDynamicLib) {
        if (publicDeps[dep.name])
            return;

        if (dep.parameters.cpp && dep.parameters.cpp.link === false)
            return;

        var isStaticLibrary = typeof dep.artifacts["staticlibrary"] !== "undefined";
        var isDynamicLibrary = !isStaticLibrary
                && typeof dep.artifacts[tagForLinkingAgainstSharedLib] !== "undefined";
        if (!isStaticLibrary && !isDynamicLibrary)
            return;
        if (isBelowIndirectDynamicLib && privateDeps[dep.name])
            return;

        var nextIsBelowIndirectDynamicLib = isBelowIndirectDynamicLib || isDynamicLibrary;
        dep.dependencies.forEach(function(depdep) {
            // If "dep" is an aggregate product, and "depdep" is one of the multiplexed variants
            // of the same product, we don't want to depend on the multiplexed variants, because
            // that could mean linking more than one time against the same library. Instead skip
            // the multiplexed dependency, and depend only on the aggregate one.
            if (depdep.name === dep.name)
                return;
            traverse(depdep, nextIsBelowIndirectDynamicLib);
        });
        if (isStaticLibrary) {
            if (!isBelowIndirectDynamicLib) {
                addArtifactFilePaths(dep, "staticlibrary", addPublicFilePath);
                if (product.cpp.importPrivateLibraries)
                    addExternalLibs(dep);
                publicDeps[dep.name] = true;
            }
        } else if (isDynamicLibrary) {
            if (!isBelowIndirectDynamicLib) {
                addArtifactFilePaths(dep, tagForLinkingAgainstSharedLib, addPublicFilePath);
                publicDeps[dep.name] = true;
            } else {
                addArtifactFilePaths(dep, tagForLinkingAgainstSharedLib, addPrivateFilePath);
                privateDeps[dep.name] = true;
            }
        }
    }

    function traverseDirectDependency(dep) {
        traverse(dep, false);
    }

    product.dependencies.forEach(traverseDirectDependency);
    addExternalLibs(product);

    var result = { libraries: [], rpath_link: [] };
    objects.forEach(
                function (obj) {
                    if (obj.direct) {
                        result.libraries.push({ filePath: obj.filePath,
                                                wholeArchive: obj.wholeArchive,
                                                symbolLinkMode: obj.symbolLinkMode,
                                                framework: obj.framework });
                    } else {
                        var dirPath = FileInfo.path(obj.filePath);
                        result.rpath_link.push(dirPath);
                    }
                });
    return result;
}

function escapeLinkerFlags(product, inputs, linkerFlags) {
    if (!linkerFlags || linkerFlags.length === 0)
        return [];

    if (useCompilerDriverLinker(product, inputs)) {
        var sep = ",";
        var useXlinker = linkerFlags.some(function (f) { return f.includes(sep); });
        if (useXlinker) {
            // One or more linker arguments contain the separator character itself
            // Use -Xlinker to handle these
            var xlinkerFlags = [];
            linkerFlags.map(function (linkerFlag) {
                if (product.cpp.enableSuspiciousLinkerFlagWarnings
                        && linkerFlag.startsWith("-Wl,")) {
                    console.warn("Encountered escaped linker flag '" + linkerFlag + "'. This may " +
                                 "cause the target to fail to link. Please do not escape these " +
                                 "flags manually; qbs does that for you.");
                }
                xlinkerFlags.push("-Xlinker", linkerFlag);
            });
            return xlinkerFlags;
        }

        if (product.cpp.enableSuspiciousLinkerFlagWarnings && linkerFlags.includes("-Xlinker")) {
            console.warn("Encountered -Xlinker linker flag escape sequence. This may cause the " +
                         "target to fail to link. Please do not escape these flags manually; " +
                         "qbs does that for you.");
        }

        // If no linker arguments contain the separator character we can just use -Wl,
        // which is more compact and easier to read in logs
        return [["-Wl"].concat(linkerFlags).join(sep)];
    }

    return linkerFlags;
}

function linkerFlags(project, product, inputs, outputs, primaryOutput, linkerPath) {
    var isDarwin = product.qbs.targetOS.includes("darwin");
    var libraryDependencies = collectLibraryDependencies(product, isDarwin);
    var rpaths = (product.cpp.useRPaths !== false) ? product.cpp.rpaths : undefined;
    var systemRunPaths = product.cpp.systemRunPaths || [];
    var canonicalSystemRunPaths = systemRunPaths.map(function(p) {
        return File.canonicalFilePath(p);
    });
    var i, args = additionalCompilerAndLinkerFlags(product);

    var escapableLinkerFlags = [];

    if (primaryOutput.fileTags.includes("dynamiclibrary")) {
        if (isDarwin) {
            args.push((function () {
                var tags = ["c", "cpp", "cppm", "objc", "objcpp", "asm_cpp"];
                for (var i = 0; i < tags.length; ++i) {
                    if (linkerPath === product.cpp.compilerPathByLanguage[tags[i]])
                        return "-dynamiclib";
                }
                return "-dylib"; // for ld64
            })());
        } else {
            args.push("-shared");
        }

        if (isDarwin) {
            if (product.cpp.internalVersion)
                args.push("-current_version", product.cpp.internalVersion);
            escapableLinkerFlags.push("-install_name", UnixUtils.soname(product,
                                                                        primaryOutput.fileName));
        } else if (product.cpp.imageFormat === "elf") {
            escapableLinkerFlags.push("-soname=" + UnixUtils.soname(product,
                                                                    primaryOutput.fileName));
        }
    }

    if (primaryOutput.fileTags.includes("loadablemodule"))
        args.push(isDarwin ? "-bundle" : "-shared");

    if (primaryOutput.fileTags.containsAny(["dynamiclibrary", "loadablemodule"])) {
        if (isDarwin)
            escapableLinkerFlags.push("-headerpad_max_install_names");
        else if (product.cpp.imageFormat === "elf")
            escapableLinkerFlags.push("--as-needed");
    }

    if (isLegacyQnxSdk(product)) {
        ["c", "cpp"].map(function (tag) {
            if (linkerPath === product.cpp.compilerPathByLanguage[tag])
                args = args.concat(qnxLangArgs(product, tag));
        });
    }

    var targetLinkerFlags = product.cpp.targetLinkerFlags;
    if (targetLinkerFlags)
        Array.prototype.push.apply(escapableLinkerFlags, targetLinkerFlags);

    var sysroot = product.cpp.syslibroot;
    if (sysroot) {
        if (product.qbs.toolchain.includes("qcc"))
            escapableLinkerFlags.push("--sysroot=" + sysroot);
        else if (isDarwin)
            escapableLinkerFlags.push("-syslibroot", sysroot);
        else
            args.push("--sysroot=" + sysroot); // do not escape, compiler-as-linker also needs it
    }

    if (product.cpp.allowUnresolvedSymbols) {
        if (isDarwin)
            escapableLinkerFlags.push("-undefined", "suppress");
        else
            escapableLinkerFlags.push("--unresolved-symbols=ignore-all");
    }

    function fixupRPath(rpath) {
        // iOS, tvOS, watchOS, and others, are fine
        if (!product.qbs.targetOS.includes("macos"))
            return rpath;

        // ...as are newer versions of macOS
        var min = product.cpp.minimumMacosVersion;
        if (min && Utilities.versionCompare(min, "10.10") >= 0)
            return rpath;

        // In older versions of dyld, a trailing slash is required
        if (!rpath.endsWith("/"))
            return rpath + "/";

        return rpath;
    }

    function isNotSystemRunPath(p) {
        return !FileInfo.isAbsolutePath(p) || (!systemRunPaths.includes(p)
                && !canonicalSystemRunPaths.includes(File.canonicalFilePath(p)));
    };

    if (!product.qbs.targetOS.includes("windows")) {
        for (i in rpaths) {
            if (isNotSystemRunPath(rpaths[i]))
                escapableLinkerFlags.push("-rpath", fixupRPath(rpaths[i]));
        }
    }

    if (product.cpp.entryPoint)
        escapableLinkerFlags.push("-e", product.cpp.entryPoint);

    if (product.qbs.toolchain.includes("mingw")) {
        if (product.consoleApplication !== undefined)
            escapableLinkerFlags.push("-subsystem",
                                      product.consoleApplication ? "console" : "windows");

        var minimumWindowsVersion = product.cpp.minimumWindowsVersion;
        if (minimumWindowsVersion) {
            // workaround for QBS-1724, mingw seems to be broken
            if (Utilities.versionCompare(minimumWindowsVersion, "6.2") > 0)
                minimumWindowsVersion = "6.2";
            var subsystemVersion = WindowsUtils.getWindowsVersionInFormat(minimumWindowsVersion, 'subsystem');
            if (subsystemVersion) {
                var major = subsystemVersion.split('.')[0];
                var minor = subsystemVersion.split('.')[1];

                // http://sourceware.org/binutils/docs/ld/Options.html
                escapableLinkerFlags.push("--major-subsystem-version", major,
                                          "--minor-subsystem-version", minor,
                                          "--major-os-version", major,
                                          "--minor-os-version", minor);
            }
        }
    }

    if (inputs.aggregate_infoplist)
        args.push("-sectcreate", "__TEXT", "__info_plist", inputs.aggregate_infoplist[0].filePath);

    var isLinkingCppObjects = !!(inputs.cpp_obj || inputs.cpp_staticlibrary);
    var stdlib = isLinkingCppObjects
            ? product.cpp.cxxStandardLibrary
            : undefined;
    if (stdlib && product.qbs.toolchain.includes("clang"))
        args.push("-stdlib=" + stdlib);

    // Flags for library search paths
    var allLibraryPaths = Cpp.collectLibraryPaths(product);
    var builtIns = product.cpp.compilerLibraryPaths
    allLibraryPaths = allLibraryPaths.filter(function(p) { return !builtIns.includes(p); });
    args = args.concat(allLibraryPaths.map(function(path) { return product.cpp.libraryPathFlag + path }));

    escapableLinkerFlags = escapableLinkerFlags.concat(Cpp.collectLinkerScriptPathsArguments(product, inputs));

    var versionScripts = inputs.versionscript
            ? inputs.versionscript.map(function(a) { return a.filePath; }) : [];
    Array.prototype.push.apply(escapableLinkerFlags, [].uniqueConcat(versionScripts)
                               .map(function(path) { return '--version-script=' + path }));

    if (isDarwin && product.cpp.warningLevel === "none")
        args.push('-w');

    var useCompilerDriver = useCompilerDriverLinker(product, inputs);
    args = args.concat(configFlags(product, useCompilerDriver));
    escapableLinkerFlags = escapableLinkerFlags.concat(Cpp.collectMiscEscapableLinkerArguments(product));

    // Note: due to the QCC response files hack in prepareLinker(), at least one object file or
    // library file must follow the output file path so that QCC has something to process before
    // sending the rest of the arguments through the response file.
    args.push("-o", primaryOutput.filePath);

    args = args.concat(Cpp.collectLinkerObjectPaths(inputs));
    args = args.concat(Cpp.collectResourceObjectPaths(inputs));

    var wholeArchiveActive = false;
    var prevLib;
    for (i = 0; i < libraryDependencies.libraries.length; ++i) {
        var dep = libraryDependencies.libraries[i];
        var lib = dep.filePath;
        if (lib === prevLib)
            continue;
        prevLib = lib;
        if (dep.wholeArchive && !wholeArchiveActive) {
            var wholeArchiveFlag;
            if (isDarwin) {
                wholeArchiveFlag = "-force_load";
            } else {
                wholeArchiveFlag = "--whole-archive";
                wholeArchiveActive = true;
            }
            Array.prototype.push.apply(args,
                                       escapeLinkerFlags(product, inputs, [wholeArchiveFlag]));
        }
        if (!dep.wholeArchive && wholeArchiveActive) {
            Array.prototype.push.apply(args,
                                       escapeLinkerFlags(product, inputs, ["--no-whole-archive"]));
            wholeArchiveActive = false;
        }

        var symbolLinkMode = dep.symbolLinkMode;
        if (isDarwin && symbolLinkMode) {
            if (!["lazy", "reexport", "upward", "weak"].includes(symbolLinkMode))
                throw new Error("unknown value '" + symbolLinkMode + "' for cpp.symbolLinkMode");
        }

        var supportsLazyMode = Utilities.versionCompare(product.cpp.compilerVersion, "15.0.0") < 0;
        if (isDarwin && symbolLinkMode && (symbolLinkMode !== "lazy" || supportsLazyMode)) {
            if (FileInfo.isAbsolutePath(lib) || lib.startsWith('@'))
                escapableLinkerFlags.push("-" + symbolLinkMode + "_library", lib);
            else if (dep.framework)
                escapableLinkerFlags.push("-" + symbolLinkMode + "_framework", lib);
            else
                escapableLinkerFlags.push("-" + symbolLinkMode + "-l" + lib);
        } else if (FileInfo.isAbsolutePath(lib) || lib.startsWith('@')) {
            args.push(dep.framework ? PathTools.frameworkExecutablePath(lib) : lib);
        } else if (dep.framework) {
            args.push("-framework", lib);
        } else {
            args.push('-l' + lib);
        }
    }
    if (wholeArchiveActive) {
        Array.prototype.push.apply(args,
                                   escapeLinkerFlags(product, inputs, ["--no-whole-archive"]));
    }
    var discardUnusedData = product.cpp.discardUnusedData;
    if (discardUnusedData !== undefined) {
        var flags = [];
        if (discardUnusedData === true) {
            if (isDarwin)
                escapableLinkerFlags.push("-dead_strip");
            else
                escapableLinkerFlags.push("--gc-sections");
        } else if (!isDarwin) {
            escapableLinkerFlags.push("--no-gc-sections");
        }
    }

    if (product.cpp.useRPathLink) {
        if (!product.cpp.rpathLinkFlag)
            throw new Error("Using rpath-link but cpp.rpathLinkFlag is not defined");
        Array.prototype.push.apply(escapableLinkerFlags, libraryDependencies.rpath_link.map(
                                       function(dir) {
                                           return product.cpp.rpathLinkFlag + dir;
                                       }));
    }

    var importLibs = outputs.dynamiclibrary_import;
    if (importLibs)
        escapableLinkerFlags.push("--out-implib", importLibs[0].filePath);

    if (outputs.application && product.cpp.generateLinkerMapFile) {
        if (isDarwin)
            escapableLinkerFlags.push("-map", outputs.mem_map[0].filePath);
        else
            escapableLinkerFlags.push("-Map=" + outputs.mem_map[0].filePath);
    }

    var escapedLinkerFlags = escapeLinkerFlags(product, inputs, escapableLinkerFlags);
    Array.prototype.push.apply(escapedLinkerFlags, args);
    if (useCompilerDriver)
        escapedLinkerFlags = escapedLinkerFlags.concat(Cpp.collectMiscLinkerArguments(product));
    if (product.qbs.toolchain.includes("mingw") && product.cpp.runtimeLibrary === "static")
        escapedLinkerFlags = ['-static-libgcc', '-static-libstdc++'].concat(escapedLinkerFlags);
    return escapedLinkerFlags;
}

// for compiler AND linker
function configFlags(config, isDriver) {
    var args = [];

    if (isDriver !== false)
        args = args.concat(Cpp.collectMiscDriverArguments(config));

    var frameworkPaths = config.cpp.frameworkPaths;
    if (frameworkPaths)
        args = args.uniqueConcat(frameworkPaths.map(function(path) { return '-F' + path }));

    var allSystemFrameworkPaths = [];

    var systemFrameworkPaths = config.cpp.systemFrameworkPaths;
    if (systemFrameworkPaths)
        allSystemFrameworkPaths = allSystemFrameworkPaths.uniqueConcat(systemFrameworkPaths);

    var distributionFrameworkPaths = config.cpp.distributionFrameworkPaths;
    if (distributionFrameworkPaths)
        allSystemFrameworkPaths = allSystemFrameworkPaths.uniqueConcat(distributionFrameworkPaths);

    args = args.concat(allSystemFrameworkPaths.map(function(path) { return '-iframework' + path }));

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
    if (!toolchain.includes("clang"))
        m["sx"] = "asm_cpp"; // clang does NOT recognize .sx
    else
        m["cppm"] = "cppm";

    return m[fileName.substring(i + 1)];
}

// Older versions of the QNX SDK have C and C++ compilers whose filenames differ only by case,
// which won't work in case insensitive environments like Win32+NTFS, HFS+ and APFS
function isLegacyQnxSdk(config) {
    return config.qbs.toolchain.includes("qcc") && config.qnx && !config.qnx.qnx7;
}

function effectiveCompilerInfo(toolchain, input, output) {
    var compilerPath, language;
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(output.fileTags));

    // Whether we're compiling a precompiled header or normal source file
    var pchOutput = output.fileTags.includes(tag + "_pch");

    var compilerPathByLanguage = input.cpp.compilerPathByLanguage;
    if (compilerPathByLanguage)
        compilerPath = compilerPathByLanguage[tag];
    if (!compilerPath
            || tag !== languageTagFromFileExtension(toolchain, input.fileName)
            || isLegacyQnxSdk(input)) {
        if (input.qbs.toolchain.includes("qcc"))
            language = qnxLangArgs(input, tag);
        else
            language = ["-x", languageName(tag) + (pchOutput ? '-header' : '')];
    }
    if (!compilerPath)
        // fall back to main compiler
        compilerPath = input.cpp.compilerPath;
    return {
        path: compilerPath,
        language: language,
        tag: tag
    };
}


function qnxLangArgs(config, tag) {
    switch (tag) {
    case "c":
        return ["-lang-c"];
    case "cpp":
        return ["-lang-c++"];
    default:
        return [];
    }
}

function handleCpuFeatures(input, flags) {
    function potentiallyAddFlagForFeature(propName, flagName) {
        var propValue = input.cpufeatures[propName];
        if (propValue === true)
            flags.push("-m" + flagName);
        else if (propValue === false)
            flags.push("-mno-" + flagName);
    }

    if (!input.qbs.architecture)
        return;
    if (input.qbs.architecture.startsWith("x86")) {
        potentiallyAddFlagForFeature("x86_avx", "avx");
        potentiallyAddFlagForFeature("x86_avx2", "avx2");
        potentiallyAddFlagForFeature("x86_avx512bw", "avx512bw");
        potentiallyAddFlagForFeature("x86_avx512cd", "avx512cd");
        potentiallyAddFlagForFeature("x86_avx512dq", "avx512dq");
        potentiallyAddFlagForFeature("x86_avx512er", "avx512er");
        potentiallyAddFlagForFeature("x86_avx512f", "avx512f");
        potentiallyAddFlagForFeature("x86_avx512ifma", "avx512ifma");
        potentiallyAddFlagForFeature("x86_avx512pf", "avx512pf");
        potentiallyAddFlagForFeature("x86_avx512vbmi", "avx512vbmi");
        potentiallyAddFlagForFeature("x86_avx512vl", "avx512vl");
        potentiallyAddFlagForFeature("x86_f16c", "f16c");
        potentiallyAddFlagForFeature("x86_sse2", "sse2");
        potentiallyAddFlagForFeature("x86_sse3", "sse3");
        potentiallyAddFlagForFeature("x86_sse4_1", "sse4.1");
        potentiallyAddFlagForFeature("x86_sse4_2", "sse4.2");
        potentiallyAddFlagForFeature("x86_ssse3", "ssse3");
    } else if (input.qbs.architecture.startsWith("arm")) {
        if (input.cpufeatures.arm_neon === true)
            flags.push("-mfpu=neon");
        if (input.cpufeatures.arm_vfpv4 === true)
            flags.push("-mfpu=vfpv4");
    } else if (input.qbs.architecture.startsWith("mips")) {
        potentiallyAddFlagForFeature("mips_dsp", "dsp");
        potentiallyAddFlagForFeature("mips_dspr2", "dspr2");
    }
}

function standardFallbackValueOrDefault(toolchain, compilerVersion, languageVersion,
                                        useLanguageVersionFallback) {
    // NEVER use the fallback values (safety brake for users in case our version map is ever wrong)
    if (useLanguageVersionFallback === false)
        return languageVersion;

    // Deprecated, but compatible with older compiler versions.
    // Note that these versions are the first to support the *value* to the -std= command line
    // option, not necessarily the first versions where support for that language standard was
    // considered fully implemented. Tested manually.
    var languageVersionsMap = {
        "c++11": {
            "fallback": "c++0x",
            "toolchains": [
                {"name": "xcode", "version": "4.3"},
                {"name": "clang", "version": "3.0"},
                {"name": "gcc", "version": "4.7"}
            ]
        },
        "c11": {
            "fallback": "c1x",
            "toolchains": [
                {"name": "xcode", "version": "5.0"},
                {"name": "clang", "version": "3.1"},
                {"name": "gcc", "version": "4.7"}
            ]
        },
        "c17": {
            "fallback": "c11",
            "toolchains": [
                {"name": "xcode", "version": "10.2"},
                {"name": "clang", "version": "7.0"},
                {"name": "gcc", "version": "8.1"}
            ]
        },
        "c2x": {
            "fallback": "c17",
            "toolchains": [
                {"name": "xcode", "version": "11.4"},
                {"name": "clang", "version": "9.0"},
                {"name": "gcc", "version": "9.0"}
            ]
        },
        "c++14": {
            "fallback": "c++1y",
            "toolchains": [
                {"name": "xcode", "version": "6.3"},
                {"name": "clang", "version": "3.5"},
                {"name": "gcc", "version": "4.9"}
            ]
        },
        "c++17": {
            "fallback": "c++1z",
            "toolchains": [
                {"name": "xcode", "version": "9.3"},
                {"name": "clang", "version": "5.0"},
                {"name": "gcc", "version": "5.1"}
            ]
        },
        "c++20": {
            "fallback": "c++2a",
            "toolchains": [
                {"name": "xcode", "version": "12.5"},
                {"name": "clang", "version": "11.0"},
                {"name": "gcc", "version": "10.1"}
            ]
        },
        "c++23": {
            "fallback": "c++2b",
            "toolchains": [
                {"name": "xcode"},
                {"name": "clang"},
                {"name": "gcc"}
            ]
        }
    };

    var m = languageVersionsMap[languageVersion];
    if (m) {
        for (var idx = 0; idx < m.toolchains.length; ++idx) {
            var tc = m.toolchains[idx];
            if (toolchain.includes(tc.name)) {
                // If we found our toolchain and it doesn't yet support the language standard
                // we're requesting, or we're using an older version that only supports the
                // preliminary flag, use that.
                if (useLanguageVersionFallback
                        || !tc.version
                        || Utilities.versionCompare(compilerVersion, tc.version) < 0)
                    return m.fallback;
                break;
            }
        }
    }

    // If we didn't find our toolchain at all, simply use the standard value.
    return languageVersion;
}

function compilerFlags(project, product, outputs, input, output, explicitlyDependsOn) {
    var i;

    // Determine which C-language we're compiling
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(output.fileTags));
    if (!["c", "cpp", "cppm", "objc", "objcpp", "asm_cpp"].includes(tag))
        throw ("unsupported source language: " + tag);

    var compilerInfo = effectiveCompilerInfo(product.qbs.toolchain,
                                             input, output);

    var args = additionalCompilerAndLinkerFlags(product);

    Array.prototype.push.apply(args, product.cpp.sysrootFlags);
    handleCpuFeatures(input, args);

    if (input.cpp.debugInformation)
        args.push('-g');
    var opt = input.cpp.optimization
    if (opt === 'fast')
        args.push('-O2');
    if (opt === 'small')
        args.push('-Os');
    if (opt === 'none')
        args.push('-O0');
    if (input.cpp.forceUseCxxModules) {
        if (!product.qbs.toolchain.includes("clang"))
            args.push('-fmodules-ts')
    }

    var warnings = input.cpp.warningLevel
    if (warnings === 'none')
        args.push('-w');
    if (warnings === 'all') {
        args.push('-Wall');
        args.push('-Wextra');
    }
    if (input.cpp.treatWarningsAsErrors)
        args.push('-Werror');

    var moduleMap = (outputs["modulemap"] || [])[0];
    if (moduleMap) {
        const moduleMapperFlag = product.qbs.toolchain.includes("clang")
            ? "@" // clang uses response file with the list of flags
            : "-fmodule-mapper="; // gcc uses file with special syntax
        args.push(moduleMapperFlag + moduleMap.filePath);
    }

    args = args.concat(configFlags(input));

    if (!input.qbs.toolchain.includes("qcc"))
        args.push('-pipe');

    if (input.cpp.enableReproducibleBuilds) {
        var toolchain = product.qbs.toolchain;
        if (!toolchain.includes("clang")) {
            var hashString = FileInfo.relativePath(project.sourceDirectory, input.filePath);
            var hash = Utilities.getHash(hashString);
            args.push("-frandom-seed=0x" + hash.substring(0, 8));
        }

        var major = product.cpp.compilerVersionMajor;
        var minor = product.cpp.compilerVersionMinor;
        if ((toolchain.includes("clang") && (major > 3 || (major === 3 && minor >= 5))) ||
            (toolchain.includes("gcc") && (major > 4 || (major === 4 && minor >= 9)))) {
            args.push("-Wdate-time");
        }
    }

    var useArc = input.cpp.automaticReferenceCounting;
    if (useArc !== undefined && (tag === "objc" || tag === "objcpp")) {
        args.push(useArc ? "-fobjc-arc" : "-fno-objc-arc");
    }

    var enableExceptions = input.cpp.enableExceptions;
    if (enableExceptions !== undefined) {
        if (tag === "cpp" || tag === "objcpp" || tag === "cppm")
            args.push(enableExceptions ? "-fexceptions" : "-fno-exceptions");

        if (tag === "objc" || tag === "objcpp") {
            args.push(enableExceptions ? "-fobjc-exceptions" : "-fno-objc-exceptions");
            if (useArc !== undefined)
                args.push(useArc ? "-fobjc-arc-exceptions" : "-fno-objc-arc-exceptions");
        }
    }

    var enableRtti = input.cpp.enableRtti;
    if (enableRtti !== undefined && (tag === "cpp" || tag === "objcpp" || tag === "cppm")) {
        args.push(enableRtti ? "-frtti" : "-fno-rtti");
    }

    var visibility = input.cpp.visibility;
    if (!product.qbs.toolchain.includes("mingw")) {
        if (visibility === 'hidden' || visibility === 'minimal')
            args.push('-fvisibility=hidden');
        if ((visibility === 'hiddenInlines' || visibility === 'minimal') && tag === 'cpp')
            args.push('-fvisibility-inlines-hidden');
        if (visibility === 'default')
            args.push('-fvisibility=default')
    }

    if (compilerInfo.language)
        // Only push language arguments if we have to.
        Array.prototype.push.apply(args, compilerInfo.language);

    args = args.concat(Cpp.collectMiscCompilerArguments(input, tag));

    var pchTag = compilerInfo.tag + "_pch";
    var pchOutput = output.fileTags.includes(pchTag);
    var pchInputs = explicitlyDependsOn[pchTag];
    if (!pchOutput && pchInputs && pchInputs.length === 1
            && ModUtils.moduleProperty(input, 'usePrecompiledHeader', tag)) {
        var pchInput = pchInputs[0];
        var pchFilePath = FileInfo.joinPaths(FileInfo.path(pchInput.filePath),
                                             pchInput.completeBaseName);
        args.push(input.cpp.preincludeFlag, pchFilePath);
    }

    args = args.concat(Cpp.collectPreincludePathsArguments(input));

    var positionIndependentCode = input.cpp.positionIndependentCode;
    if (positionIndependentCode && !product.qbs.targetOS.includes("windows"))
        args.push('-fPIC');

    var cppFlags = input.cpp.cppFlags;
    for (i in cppFlags)
        args.push('-Wp,' + cppFlags[i])

    args = args.concat(Cpp.collectDefinesArguments(input));
    args = args.concat(Cpp.collectIncludePathsArguments(input));
    args = args.concat(Cpp.collectSystemIncludePathsArguments(input));

    var minimumWindowsVersion = input.cpp.minimumWindowsVersion;
    if (minimumWindowsVersion && product.qbs.targetOS.includes("windows")) {
        var hexVersion = WindowsUtils.getWindowsVersionInFormat(minimumWindowsVersion, 'hex');
        if (hexVersion) {
            var versionDefs = [ 'WINVER', '_WIN32_WINNT', '_WIN32_WINDOWS' ];
            for (i in versionDefs)
                args.push(input.cpp.defineFlag + versionDefs[i] + '=' + hexVersion);
        }
    }

    function currentLanguageVersion(tag) {
        switch (tag) {
        case "c":
        case "objc":
            var knownValues = ["c2x", "c17", "c11", "c99", "c90", "c89"];
            return Cpp.languageVersion(input.cpp.cLanguageVersion, knownValues, "C");
        case "cpp":
        case "cppm":
        case "objcpp":
            knownValues = ["c++23", "c++2b", "c++20", "c++2a", "c++17", "c++1z",
                           "c++14", "c++1y", "c++11", "c++0x",
                           "c++03", "c++98"];
            return Cpp.languageVersion(input.cpp.cxxLanguageVersion, knownValues, "C++");
        }
    }

    var langVersion = currentLanguageVersion(tag);
    if (langVersion) {
        args.push("-std=" + standardFallbackValueOrDefault(product.qbs.toolchain,
                                                           product.cpp.compilerVersion,
                                                           langVersion,
                                                           product.cpp.useLanguageVersionFallback));
    }

    if (tag === "cpp" || tag === "objcpp" || tag === "cppm") {
        var cxxStandardLibrary = product.cpp.cxxStandardLibrary;
        if (cxxStandardLibrary && product.qbs.toolchain.includes("clang")) {
            args.push("-stdlib=" + cxxStandardLibrary);
        }
    }

    args.push("-o", output.filePath);
    args.push("-c", input.filePath);

    return args;
}

function additionalCompilerAndLinkerFlags(product) {
    var args = []

    var requireAppExtensionSafeApi = product.cpp.requireAppExtensionSafeApi;
    if (requireAppExtensionSafeApi !== undefined && product.qbs.targetOS.includes("darwin")) {
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
    else if (fileTag === 'cppm')
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
    var assemblerPath = product.cpp.assemblerPath;

    var args = product.cpp.targetAssemblerFlags;

    if (input.cpp.debugInformation)
        args.push('-g');

    var warnings = input.cpp.warningLevel
    if (warnings === 'none')
        args.push('-W');

    args = args.concat(Cpp.collectMiscAssemblerArguments(input, "asm"));
    args = args.concat(Cpp.collectIncludePathsArguments(input));
    args = args.concat(Cpp.collectSystemIncludePathsArguments(input, input.cpp.includeFlag));

    args.push("-o", output.filePath);
    args.push(input.filePath);

    var cmd = new Command(assemblerPath, args);
    cmd.description = "assembling " + input.fileName;
    cmd.highlight = "compiler";
    cmd.jobPool = "assembler";
    return cmd;
}

function compilerEnvVars(config, compilerInfo)
{
    if (config.qbs.toolchain.includes("qcc"))
        return ["QCC_CONF_PATH"];

    var list = ["CPATH", "TMPDIR"];
    if (compilerInfo.tag === "c")
        list.push("C_INCLUDE_PATH");
    else if (compilerInfo.tag === "cpp")
        list.push("CPLUS_INCLUDE_PATH");
    else if (compilerInfo.tag === "cppm")
        list.push("CPLUS_INCLUDE_PATH");
    else if (compilerInfo.tag === "objc")
        list.push("OBJC_INCLUDE_PATH");
    else if (compilerInfo.tag === "objccpp")
        list.push("OBJCPLUS_INCLUDE_PATH");
    if (config.qbs.targetOS.includes("macos"))
        list.push("MACOSX_DEPLOYMENT_TARGET");
    else if (config.qbs.targetOS.includes("ios"))
        list.push("IPHONEOS_DEPLOYMENT_TARGET");
    else if (config.qbs.targetOS.includes("tvos"))
        list.push("TVOS_DEPLOYMENT_TARGET");
    else if (config.qbs.targetOS.includes("watchos"))
        list.push("WATCHOS_DEPLOYMENT_TARGET");
    if (config.qbs.toolchain.includes("clang")) {
        list.push("TEMP", "TMP");
    } else {
        list.push("LANG", "LC_CTYPE", "LC_MESSAGES", "LC_ALL", "GCC_COMPARE_DEBUG",
                  "GCC_EXEC_PREFIX", "COMPILER_PATH", "SOURCE_DATE_EPOCH");
    }
    return list;
}

function linkerEnvVars(config, inputs)
{
    if (config.qbs.toolchain.includes("clang") || config.qbs.toolchain.includes("qcc"))
        return [];
    var list = ["GNUTARGET", "LDEMULATION", "COLLECT_NO_DEMANGLE"];
    if (useCompilerDriverLinker(config, inputs))
        list.push("LIBRARY_PATH");
    return list;
}

function setResponseFileThreshold(command, product)
{
    if (Host.os().includes("windows"))
        command.responseFileThreshold = 8000;
}

function prepareCompilerInternal(project, product, inputs, outputs, input, output_, explicitlyDependsOn) {
    var output = output_ || outputs["obj"][0];

    var compilerInfo = effectiveCompilerInfo(product.qbs.toolchain,
                                             input, output);

    var compilerPath = compilerInfo.path;
    var pchOutput = output.fileTags.includes(compilerInfo.tag + "_pch");

    var args = compilerFlags(project, product, outputs, input, output, explicitlyDependsOn);

    var wrapperArgsLength = 0;
    var wrapperArgs = product.cpp.compilerWrapper;
    var extraEnv;
    if (wrapperArgs && wrapperArgs.length > 0) {

        // distcc cannot deal with absolute compiler paths (QBS-1336).
        for (var i = 0; i < wrapperArgs.length; ++i) {
            if (FileInfo.baseName(wrapperArgs[i]) !== "distcc")
                continue;
            if (i === wrapperArgs.length - 1) {
                if (FileInfo.isAbsolutePath(compilerPath)) {
                    extraEnv = ["PATH=" + FileInfo.path(compilerPath)];
                    compilerPath = FileInfo.fileName(compilerPath);
                }
            } else if (FileInfo.isAbsolutePath(wrapperArgs[i + 1])) {
                extraEnv = ["PATH=" + FileInfo.path(FileInfo.path(wrapperArgs[i + 1]))];
                wrapperArgs[i + 1] = FileInfo.fileName(wrapperArgs[i + 1]);
            }
            break;
        }

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
    cmd.jobPool = "compiler";
    cmd.relevantEnvironmentVariables = compilerEnvVars(input, compilerInfo);
    if (extraEnv)
        cmd.environment = extraEnv;
    cmd.responseFileArgumentIndex = wrapperArgsLength;
    cmd.responseFileUsagePrefix = '@';
    setResponseFileThreshold(cmd, product);
    return cmd;
}

function prepareCompiler(project, product, inputs, outputs, input, output, explicitlyDependsOn) {
    var result = Cpp.prepareModules(project, product, inputs, outputs, input, output);
    result = result.concat(prepareCompilerInternal(
        project, product, inputs, outputs, input, output, explicitlyDependsOn));
    return result;
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

function collectStdoutLines(command, args)
{
    var p = new Process();
    try {
        p.exec(command, args);
        return p.readStdOut().split(/\r?\n/g).filter(function (e) { return e; });
    } finally {
        p.close();
    }
}

function getSymbolInfo(product, inputFile)
{
    var result = { };
    var command = product.cpp.nmPath;
    var args = ["-g", "-P"];
    if (product.cpp._nmHasDynamicOption)
        args.push("-D");
    try {
        result.allGlobalSymbols = collectStdoutLines(command, args.concat(inputFile));

        // GNU nm has the "--defined" option but POSIX nm does not, so we have to manually
        // construct the list of defined symbols by subtracting.
        var undefinedGlobalSymbols = collectStdoutLines(command, args.concat(["-u", inputFile]));
        result.definedGlobalSymbols = result.allGlobalSymbols.filter(function(line) {
            return !undefinedGlobalSymbols.includes(line); });
        result.success = true;
    } catch (e) {
        console.debug("Failed to collect symbols for shared library: nm command '"
                      + command + "' failed (" + e.toString() + ")");
        result.success = false;
    }
    return result;
}

function createSymbolFile(filePath, allSymbols, definedSymbols)
{
    var file;
    try {
        file = new TextFile(filePath, TextFile.WriteOnly);
        for (var lineNr in allSymbols)
            file.writeLine(allSymbols[lineNr]);
        file.writeLine("===");
        for (lineNr in definedSymbols)
            file.writeLine(definedSymbols[lineNr]);
    } finally {
        if (file)
            file.close();
    }
}

function readSymbolFile(filePath)
{
    var result = { success: true, allGlobalSymbols: [], definedGlobalSymbols: [] };
    var file;
    try {
        file = new TextFile(filePath, TextFile.ReadOnly);
        var prop = "allGlobalSymbols";
        while (true) {
            var line = file.readLine();
            if (!line)
                break;
            if (line === "===") {
                prop = "definedGlobalSymbols";
                continue;
            }
            result[prop].push(line);
        }
    } catch (e) {
        console.debug("Failed to read symbols from '" + filePath + "'");
        result.success = false;
    } finally {
        if (file)
            file.close();
    }
    return result;
}

function createSymbolCheckingCommands(product, outputs) {
    var commands = [];
    if (!outputs.dynamiclibrary || !outputs.dynamiclibrary_symbols)
        return commands;

    if (outputs.dynamiclibrary.length !== outputs.dynamiclibrary_symbols.length)
        throw new Error("The number of outputs tagged dynamiclibrary ("
                        + outputs.dynamiclibrary.length + ") must be equal to the number of "
                        + "outputs tagged dynamiclibrary_symbols ("
                        + outputs.dynamiclibrary_symbols.length + ")");

    for (var d = 0; d < outputs.dynamiclibrary_symbols.length; ++d) {
        // Update the symbols file if the list of relevant symbols has changed.
        var cmd = new JavaScriptCommand();
        cmd.silent = true;
        cmd.d = d;
        cmd.sourceCode = function() {
            if (outputs.dynamiclibrary[d].qbs.buildVariant
                    !== outputs.dynamiclibrary_symbols[d].qbs.buildVariant)
                throw new Error("Build variant of output tagged dynamiclibrary ("
                                + outputs.dynamiclibrary[d].qbs.buildVariant + ") is not equal to "
                                + "build variant of output tagged dynamiclibrary_symbols ("
                                + outputs.dynamiclibrary_symbols[d].qbs.buildVariant + ") at index "
                                + d);

            var libFilePath = outputs.dynamiclibrary[d].filePath;
            var symbolFilePath = outputs.dynamiclibrary_symbols[d].filePath;

            var newNmResult = getSymbolInfo(product, libFilePath);
            if (!newNmResult.success)
                return;

            if (!File.exists(symbolFilePath)) {
                console.debug("Symbol file '" + symbolFilePath + "' does not yet exist.");
                createSymbolFile(symbolFilePath, newNmResult.allGlobalSymbols,
                                 newNmResult.definedGlobalSymbols);
                return;
            }

            var oldNmResult = readSymbolFile(symbolFilePath);
            var checkMode = product.cpp.exportedSymbolsCheckMode;
            var oldSymbols;
            var newSymbols;
            if (checkMode === "strict") {
                oldSymbols = oldNmResult.allGlobalSymbols;
                newSymbols = newNmResult.allGlobalSymbols;
            } else {
                var weakFilter = function(line) {
                    var symbolType = line.split(/\s+/)[1];
                    return symbolType != "v" && symbolType != "V"
                            && symbolType != "w" && symbolType != "W";
                };
                oldSymbols = oldNmResult.definedGlobalSymbols.filter(weakFilter);
                newSymbols = newNmResult.definedGlobalSymbols.filter(weakFilter);
            }
            if (oldSymbols.length !== newSymbols.length) {
                console.debug("List of relevant symbols differs for '" + libFilePath + "'.");
                createSymbolFile(symbolFilePath, newNmResult.allGlobalSymbols,
                                 newNmResult.definedGlobalSymbols);
                return;
            }
            for (var i = 0; i < oldSymbols.length; ++i) {
                var oldLine = oldSymbols[i];
                var newLine = newSymbols[i];
                var oldLineElems = oldLine.split(/\s+/);
                var newLineElems = newLine.split(/\s+/);
                if (oldLineElems[0] !== newLineElems[0] // Object name.
                        || oldLineElems[1] !== newLineElems[1]) { // Object type
                    console.debug("List of relevant symbols differs for '" + libFilePath + "'.");
                    createSymbolFile(symbolFilePath, newNmResult.allGlobalSymbols,
                                     newNmResult.definedGlobalSymbols);
                    return;
                }
            }
        }
        commands.push(cmd);
    }
    return commands;
}

function separateDebugInfoCommands(product, outputs, primaryOutput) {
    var commands = [];

    var debugInfo = outputs.debuginfo_app || outputs.debuginfo_dll
            || outputs.debuginfo_loadablemodule;

    if (debugInfo && !product.qbs.toolchain.includes("emscripten")) {
        var objcopy = product.cpp.objcopyPath;

        var cmd = new Command(objcopy, ["--only-keep-debug", primaryOutput.filePath,
                                    debugInfo[0].filePath]);
        cmd.silent = true;
        commands.push(cmd);

        cmd = new Command(objcopy, ["--strip-debug", primaryOutput.filePath]);
        cmd.silent = true;
        commands.push(cmd);

        cmd = new Command(objcopy, ["--add-gnu-debuglink=" + debugInfo[0].filePath,
                                    primaryOutput.filePath]);
        cmd.silent = true;
        commands.push(cmd);
    }

    return commands;
}

function separateDebugInfoCommandsDarwin(product, outputs, primaryOutputs) {
    var commands = [];

    var debugInfo = outputs.debuginfo_app || outputs.debuginfo_dll
            || outputs.debuginfo_loadablemodule;
    if (debugInfo) {
        var dsymPath = debugInfo[0].filePath;
        if (outputs.debuginfo_bundle && outputs.debuginfo_bundle[0])
            dsymPath = outputs.debuginfo_bundle[0].filePath;

        var flags = product.cpp.dsymutilFlags || [];
        var files = primaryOutputs.map(function (f) { return f.filePath; });
        var cmd = new Command(product.cpp.dsymutilPath,
                              flags.concat(["-o", dsymPath].concat(files)));
        cmd.description = "generating dSYM for " + product.name;
        commands.push(cmd);

        // strip debug info
        cmd = new Command(product.cpp.stripPath, ["-S"].concat(files));
        cmd.silent = true;
        commands.push(cmd);
    }

    return commands;
}

function librarySymlinkArtifacts(product, buildVariantSuffix) {
    var artifacts = [];
    if (product.cpp.shouldCreateSymlinks && (!product.bundle || !product.bundle.isBundle)) {
        var maxVersionParts = product.cpp.internalVersion ? 3 : 1;
        var primaryFileName =
            PathTools.dynamicLibraryFilePath(product, buildVariantSuffix, undefined);
        for (var i = 0; i < maxVersionParts; ++i) {
            var symlink = {
                filePath: FileInfo.joinPaths(product.destinationDirectory,
                                             PathTools.dynamicLibraryFilePath(
                                                 product, buildVariantSuffix, undefined, i)),
                fileTags: ["dynamiclibrary_symlink"],
                cpp: { primaryFileName: primaryFileName }
            };
            if (i > 0 && artifacts[i-1].filePath == symlink.filePath)
                break; // Version number has less than three components.
            artifacts.push(symlink);
        }
    }
    return artifacts;
}

function librarySymlinkCommands(outputs, primaryOutput) {
    var commands = [];
    // Create symlinks from {libfoo, libfoo.1, libfoo.1.0} to libfoo.1.0.0
    var links = outputs["dynamiclibrary_symlink"];
    var symlinkCount = links ? links.length : 0;
    for (i = 0; i < symlinkCount; ++i) {
        var cmd = new Command("ln", ["-sf", links[i].cpp.primaryFileName, links[i].filePath]);
        cmd.highlight = "filegen";
        cmd.description = "creating symbolic link '" + links[i].fileName + "'";
        cmd.workingDirectory = FileInfo.path(primaryOutput.filePath);
        commands.push(cmd);
    }
    return commands;
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

    var linkerPath = effectiveLinkerPath(product, inputs)

    var args = linkerFlags(project, product, inputs, outputs, primaryOutput, linkerPath);
    var wrapperArgsLength = 0;
    var wrapperArgs = product.cpp.linkerWrapper;
    if (wrapperArgs && wrapperArgs.length > 0) {
        wrapperArgsLength = wrapperArgs.length;
        args.unshift(linkerPath);
        linkerPath = wrapperArgs.shift();
        args = wrapperArgs.concat(args);
    }

    var responseFileArgumentIndex = wrapperArgsLength;

    // qcc doesn't properly handle response files, so we have to do it manually
    var useQnxResponseFileHack = product.qbs.toolchain.includes("qcc")
            && useCompilerDriverLinker(product, inputs);
    if (useQnxResponseFileHack) {
        // qcc needs to see at least one object/library file to think it has something to do,
        // so start the response file at the second object file (so, 3 after the last -o option)
        var idx = args.lastIndexOf("-o");
        if (idx !== -1 && idx + 3 < args.length)
            responseFileArgumentIndex += idx + 3;
    }

    cmd = new Command(linkerPath, args);
    cmd.description = 'linking ' + primaryOutput.fileName;
    cmd.highlight = 'linker';
    cmd.jobPool = "linker";
    cmd.relevantEnvironmentVariables = linkerEnvVars(product, inputs);
    cmd.responseFileArgumentIndex = responseFileArgumentIndex;
    cmd.responseFileUsagePrefix = useQnxResponseFileHack ? "-Wl,@" : "@";
    setResponseFileThreshold(cmd, product);
    commands.push(cmd);

    if (product.qbs.toolchain.includes("emscripten") && outputs.application
            && product.cpp.separateDebugInformation) {
        args.push("-gseparate-dwarf");
    } else if (product.qbs.targetOS.includes("darwin")) {
        if (!product.aggregate) {
            commands = commands.concat(separateDebugInfoCommandsDarwin(
                                           product, outputs, [primaryOutput]));
        }
    } else {
        commands = commands.concat(separateDebugInfoCommands(product, outputs, primaryOutput));
    }

    if (outputs.dynamiclibrary) {
        Array.prototype.push.apply(commands, createSymbolCheckingCommands(product, outputs));
        Array.prototype.push.apply(commands, librarySymlinkCommands(outputs, primaryOutput));
    }

    if (product.cpp.shouldSignArtifacts) {
        Array.prototype.push.apply(
                commands, Codesign.prepareSign(project, product, inputs, outputs, input, output));
    }

    return commands;
}

function debugInfoArtifacts(product, variants, debugInfoTagSuffix) {
    var fileTag;
    switch (debugInfoTagSuffix) {
    case "app":
        fileTag = "application";
        break;
    case "dll":
        fileTag = "dynamiclibrary";
        break;
    default:
        fileTag = debugInfoTagSuffix;
        break;
    }

    variants = variants || [{}];

    var artifacts = [];
    var separateDebugInfo = product.cpp.separateDebugInformation;
    if (separateDebugInfo && product.qbs.toolchain.includes("emscripten"))
        separateDebugInfo = fileTag === "application";
    if (separateDebugInfo) {
        variants.map(function (variant) {
            artifacts.push({
                filePath: FileInfo.joinPaths(product.destinationDirectory,
                                                 PathTools.debugInfoFilePath(product,
                                                                             variant.suffix,
                                                                             fileTag)),
                fileTags: ["debuginfo_" + debugInfoTagSuffix]
            });
        });
        if (PathTools.debugInfoIsBundle(product)) {
            artifacts.push({
                filePath: FileInfo.joinPaths(product.destinationDirectory,
                                             PathTools.debugInfoBundlePath(product, fileTag)),
                fileTags: ["debuginfo_bundle"]
            });
            artifacts.push({
                filePath: FileInfo.joinPaths(product.destinationDirectory,
                                             PathTools.debugInfoPlistFilePath(product, fileTag)),
                fileTags: ["debuginfo_plist"]
            });
        }
    }
    return artifacts;
}

function dumpMacros(env, compilerFilePath, args, nullDevice, tag) {
    var p = new Process();
    try {
        p.setEnv("LC_ALL", "C");
        for (var key in env)
            p.setEnv(key, env[key]);
        // qcc NEEDS the explicit -Wp, prefix to -dM; clang and gcc do not but all three accept it
        p.exec(compilerFilePath,
               (args || []).concat(["-Wp,-dM", "-E", "-x", languageName(tag || "c") , nullDevice]),
               true);
        return Cpp.extractMacros(p.readStdOut());
    } finally {
        p.close();
    }
}

function dumpDefaultPaths(env, compilerFilePath, args, nullDevice, pathListSeparator, sysroot,
                          targetOS) {
    var p = new Process();
    try {
        p.setEnv("LC_ALL", "C");
        for (var key in env)
            p.setEnv(key, env[key]);
        args = args || [];
        p.exec(compilerFilePath, args.concat(["-v", "-E", "-x", "c++", nullDevice]), true);
        var suffix = " (framework directory)";
        var includePaths = [];
        var libraryPaths = [];
        var frameworkPaths = [];
        var addIncludes = false;
        var lines = p.readStdErr().trim().split(/\r?\n/g).map(function (line) { return line.trim(); });
        for (var i = 0; i < lines.length; ++i) {
            var line = lines[i];
            var prefix = "LIBRARY_PATH=";
            if (line.startsWith(prefix)) {
                libraryPaths = libraryPaths.concat(line.substr(prefix.length)
                                                   .split(pathListSeparator));
            } else if (line === "#include <...> search starts here:") {
                addIncludes = true;
            } else if (line === "End of search list.") {
                addIncludes = false;
            } else if (addIncludes) {
                if (line.endsWith(suffix))
                    frameworkPaths.push(line.substr(0, line.length - suffix.length));
                else
                    includePaths.push(line);
            }
        }

        sysroot = sysroot || "";

        if (includePaths.length === 0)
            includePaths.push(sysroot + "/usr/include", sysroot + "/usr/local/include");

        if (libraryPaths.length === 0)
            libraryPaths.push(sysroot + "/lib", sysroot + "/usr/lib");

        if (frameworkPaths.length === 0 && targetOS.includes("darwin"))
            frameworkPaths.push(sysroot + "/System/Library/Frameworks");

        return {
            "includePaths": includePaths,
            "libraryPaths": libraryPaths,
            "frameworkPaths": frameworkPaths
        };
    } finally {
        p.close();
    }
}

function targetLinkerFlags(targetArch, targetOS) {
    var linkerFlags = {
        "windows": {
            "i386": "i386pe",
            "x86_64": "i386pep",
        },
        "freebsd": {
            "i386": "elf_i386_fbsd",
            "x86_64": "elf_x86_64_fbsd",
        },
        "other": {
            "i386": "elf_i386",
            "x86_64": "elf_x86_64",
        }
    };
    if (targetOS.includes("windows"))
        return linkerFlags["windows"][targetArch];
    else if (targetOS.includes("freebsd"))
        return linkerFlags["freebsd"][targetArch];
    return linkerFlags["other"][targetArch];
}

function targetFlags(tool, hasTargetOption, target, targetArch, machineType, targetOS) {
    var args = [];
    if (hasTargetOption) {
        if (target)
            args.push("-target", target);
    } else {
        var archArgs = {
            "compiler": {
                "i386": ["-m32"],
                "x86_64": ["-m64"],
            },
            "linker": {
                "i386": ["-m", targetLinkerFlags("i386", targetOS)],
                "x86_64": ["-m", targetLinkerFlags("x86_64", targetOS)],
            },
            "assembler": {
                "i386": ["--32"],
                "x86_64": ["--64"],
            },
        };

        var flags = archArgs[tool] ? archArgs[tool][targetArch] : undefined;
        if (flags)
            args = args.concat(flags);

        if (machineType && tool !== "linker")
            args.push("-march=" + machineType);
    }
    return args;
}

function toolNames(rawToolNames, toolchainPrefix)
{
    return toolchainPrefix
            ? rawToolNames.map(function(rawName) { return toolchainPrefix + rawName; })
            : rawToolNames;
}

function pathPrefix(baseDir, prefix)
{
    var path = '';
    if (baseDir) {
        path += baseDir;
        if (path.substr(-1) !== '/')
            path += '/';
    }
    if (prefix)
        path += prefix;
    return path;
}

function appLinkerOutputArtifacts(product)
{
    var app = {
        filePath: FileInfo.joinPaths(product.destinationDirectory,
                                     PathTools.applicationFilePath(product)),
        fileTags: ["bundle.input", "application"]
            .concat(product.cpp.isForMainBundle ? ["bundle.main.input", "bundle.main.executable"] : [])
            .concat(product.cpp.shouldSignArtifacts ? ["codesign.signed_artifact"] : []),
        bundle: {
            _bundleFilePath: FileInfo.joinPaths(product.destinationDirectory,
                                                PathTools.bundleExecutableFilePath(product))
        }
    }
    var artifacts = [app];
    if (!product.aggregate)
        artifacts = artifacts.concat(debugInfoArtifacts(product, undefined, "app"));
    if (product.cpp.generateLinkerMapFile) {
        artifacts.push({
            filePath: FileInfo.joinPaths(product.destinationDirectory,
                                         product.targetName + product.cpp.linkerMapSuffix),
            fileTags: ["mem_map"]
        });
    }
    if (product.qbs.toolchain.includes("emscripten"))
        artifacts = artifacts.concat(wasmArtifacts(product));
    return artifacts;
}

function moduleLinkerOutputArtifacts(product, inputs)
{
    var app = {
        filePath: FileInfo.joinPaths(product.destinationDirectory,
                                     PathTools.loadableModuleFilePath(product)),
        fileTags: ["bundle.input", "loadablemodule"]
            .concat(product.cpp.isForMainBundle ? ["bundle.main.input", "bundle.main.plugin"] : [])
            .concat(product.cpp.shouldSignArtifacts ? ["codesign.signed_artifact"] : []),
        bundle: {
            _bundleFilePath: FileInfo.joinPaths(product.destinationDirectory,
                                                PathTools.bundleExecutableFilePath(product))
        }
    }
    var artifacts = [app];
    if (!product.aggregate)
        artifacts = artifacts.concat(debugInfoArtifacts(product, undefined,
                                                        "loadablemodule"));
    return artifacts;
}

function staticLibLinkerOutputArtifacts(product)
{
    var tags = ["bundle.input", "staticlibrary"]
        .concat(product.cpp.isForMainBundle ? ["bundle.main.input", "bundle.main.library"] : [])
        .concat(product.cpp.shouldSignArtifacts ? ["codesign.signed_artifact"] : []);
    var objs = inputs["obj"];
    var objCount = objs ? objs.length : 0;
    for (var i = 0; i < objCount; ++i) {
        var ft = objs[i].fileTags;
        if (ft.includes("c_obj"))
            tags.push("c_staticlibrary");
        if (ft.includes("cpp_obj"))
            tags.push("cpp_staticlibrary");
    }
    return [{
        filePath: FileInfo.joinPaths(product.destinationDirectory,
                                     PathTools.staticLibraryFilePath(product)),
        fileTags: tags,
        bundle: {
            _bundleFilePath: FileInfo.joinPaths(product.destinationDirectory,
                                                PathTools.bundleExecutableFilePath(product))
        }
    }];
}

function staticLibLinkerCommands(project, product, inputs, outputs, input, output,
                                 explicitlyDependsOn)
{
    var commands = [];
    var args = ['rcs'];
    Array.prototype.push.apply(args, product.cpp.archiverFlags);
    args.push(output.filePath);
    for (var i in inputs.obj)
        args.push(inputs.obj[i].filePath);
    for (var i in inputs.res)
        args.push(inputs.res[i].filePath);
    var cmd = new Command(product.cpp.archiverPath, args);
    cmd.description = 'creating ' + output.fileName;
    cmd.highlight = 'linker'
    cmd.jobPool = "linker";
    cmd.responseFileUsagePrefix = '@';
    setResponseFileThreshold(cmd, product);
    commands.push(cmd);

    if (product.cpp.shouldSignArtifacts) {
        Array.prototype.push.apply(
            commands, Codesign.prepareSign(project, product, inputs, outputs, input, output));
    }
    return commands;
}

function dynamicLibLinkerOutputArtifacts(product)
{
    var artifacts = [{
        filePath: FileInfo.joinPaths(product.destinationDirectory,
                                     PathTools.dynamicLibraryFilePath(product)),
        fileTags: ["bundle.input", "dynamiclibrary"]
            .concat(product.cpp.isForMainBundle ? ["bundle.main.input", "bundle.main.library"] : [])
            .concat(product.cpp.shouldSignArtifacts ? ["codesign.signed_artifact"] : []),
        bundle: {
            _bundleFilePath: FileInfo.joinPaths(product.destinationDirectory,
                                                PathTools.bundleExecutableFilePath(product))
        }
    }];
    if (product.cpp.imageFormat === "pe") {
        artifacts.push({
            fileTags: ["dynamiclibrary_import"],
            filePath: FileInfo.joinPaths(product.destinationDirectory,
                                         PathTools.importLibraryFilePath(product)),
            alwaysUpdated: false
        });
    } else {
        // List of libfoo's public symbols for smart re-linking.
        artifacts.push({
            filePath: product.destinationDirectory + "/.sosymbols/"
                      + PathTools.dynamicLibraryFilePath(product),
            fileTags: ["dynamiclibrary_symbols"],
            alwaysUpdated: false,
        });
    }

    artifacts = artifacts.concat(librarySymlinkArtifacts(product));
    if (!product.aggregate)
        artifacts = artifacts.concat(debugInfoArtifacts(product, undefined, "dll"));
    return artifacts;
}

function wasmArtifacts(product)
{
    var flags = product.cpp.driverLinkerFlags;
    var wasmoption = 1;
    var pthread = false;
    for (var index in flags) {
        var option = flags[index];
        if (option.indexOf("WASM") !== -1) {
            option = option.trim();
            wasmoption = option.substring(option.length - 1);
        } else if (option.indexOf("-pthread") !== -1) {
            pthread = true;
        }
    }

    var artifacts = [];
    var createAppArtifact = function(fileName) {
        return {
            filePath: FileInfo.joinPaths(product.destinationDirectory, fileName),
            fileTags: ["wasm"]
        };
    };
    var suffix = product.cpp.executableSuffix;
    if (suffix !== ".wasm") {
        if (suffix === ".html")
            artifacts.push(createAppArtifact(product.targetName + ".js"));
        if (pthread)
            artifacts.push(createAppArtifact(product.targetName + ".worker.js"));
    }

    if (wasmoption !== 0 && suffix !== ".wasm") //suffix .wasm will already result in "application".wasm
        artifacts.push(createAppArtifact(product.targetName + ".wasm"));
    if (wasmoption == 2)
        artifacts.push(createAppArtifact(product.targetName + ".wasm.js"));

      return artifacts;
}

function configureStdModules() {
    try {
        var probe = new Process();
        // check which stdlib is used? this actually works:
        // clang-18 -print-file-name=libstdc++.modules.json --sysroot /opt/gcc-15/
        const modulesJsonFiles = [
            "libc++.modules.json",
            "libstdc++.modules.json",
            "../../c++/libc++.modules.json",
        ];
        for (var i = 0; i < modulesJsonFiles.length; ++i) {
            const modulesJsonFile = modulesJsonFiles[i];
            const result = probe.exec(_compilerPath, ["-print-file-name=" + modulesJsonFile], false);
            if (result !== 0)
                continue;
            const path = probe.readStdOut().trim();
            if (path !== modulesJsonFile && FileInfo.isAbsolutePath(path) && File.exists(path)) {
                const jsonFile = new TextFile(path, TextFile.ReadOnly);

                const json = JSON.parse(jsonFile.readAll());
                jsonFile.close();
                const modules = json.modules
                    .filter(function(module) {
                        const logicalName = module["logical-name"];
                        if (logicalName === "std" && (_forceUseImportStd || _forceUseImportStdCompat)) {
                            return true;
                        } else if (logicalName === "std.compat" && _forceUseImportStdCompat) {
                            return true;
                        }
                        return false;
                    })
                    .map(function(module) {
                        const sourcePath = module["source-path"];
                        if (FileInfo.isAbsolutePath(sourcePath))
                            return sourcePath;
                        return FileInfo.joinPaths(FileInfo.path(path), sourcePath);
                    })
                    .filter(function(module) {
                        return File.exists(module);
                    });
                if (modules.length > 0) {
                    found = true;
                    _stdModulesFiles = modules;
                    break;
                }
            }
        }
    } finally {
        probe.close();
    }
}
