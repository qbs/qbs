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

var File = require("qbs.File");
var FileInfo = require("qbs.FileInfo");
var ModUtils = require("qbs.ModUtils");
var WindowsUtils = require("qbs.WindowsUtils");

function compilerVersionDefine(cpp) {
    var result = '_MSC_VER=' + cpp.compilerVersionMajor;
    var s = cpp.compilerVersionMinor.toString();
    while (s.length < 2)
        s = '0' + s;
    result += s;
    return result;
}

function prepareCompiler(project, product, inputs, outputs, input, output) {
    var i;
    var debugInformation = input.cpp.debugInformation;
    var args = ['/nologo', '/c']

    // Determine which C-language we're compiling
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(Object.keys(outputs)));
    if (!["c", "cpp"].contains(tag))
        throw ("unsupported source language");

    // Whether we're compiling a precompiled header or normal source file
    var pchOutput = outputs[tag + "_pch"] ? outputs[tag + "_pch"][0] : undefined;

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

    // warnings:
    var warningLevel = input.cpp.warningLevel;
    if (warningLevel === 'none')
        args.push('/w')
    if (warningLevel === 'all')
        args.push('/Wall')
    if (input.cpp.treatWarningsAsErrors)
        args.push('/WX')
    var allIncludePaths = [];
    var includePaths = input.cpp.includePaths;
    if (includePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(includePaths);
    var systemIncludePaths = input.cpp.systemIncludePaths;
    if (systemIncludePaths)
        allIncludePaths = allIncludePaths.uniqueConcat(systemIncludePaths);
    for (i in allIncludePaths)
        args.push('/I' + FileInfo.toWindowsSeparators(allIncludePaths[i]))
    var allDefines = [];
    var platformDefines = input.cpp.platformDefines;
    if (platformDefines)
        allDefines = allDefines.uniqueConcat(platformDefines);
    var defines = input.cpp.defines;
    if (defines)
        allDefines = allDefines.uniqueConcat(defines);
    for (i in allDefines)
        args.push('/D' + allDefines[i].replace(/%/g, "%%"));

    var minimumWindowsVersion = product.cpp.minimumWindowsVersion;
    if (minimumWindowsVersion) {
        var hexVersion = WindowsUtils.getWindowsVersionInFormat(minimumWindowsVersion, 'hex');
        if (hexVersion) {
            var versionDefs = [ 'WINVER', '_WIN32_WINNT', '_WIN32_WINDOWS' ];
            for (i in versionDefs) {
                args.push('/D' + versionDefs[i] + '=' + hexVersion);
            }
        }
    }

    var objOutput = outputs.obj ? outputs.obj[0] : undefined
    args.push('/Fo' + FileInfo.toWindowsSeparators(objOutput.filePath))
    args.push(FileInfo.toWindowsSeparators(input.filePath))

    var prefixHeaders = product.cpp.prefixHeaders;
    for (i in prefixHeaders)
        args.push("/FI" + FileInfo.toWindowsSeparators(prefixHeaders[i]));

    // Language
    if (tag === "cpp")
        args.push("/TP");
    else if (tag === "c")
        args.push("/TC");

    var commands = [];
    var usePch = ModUtils.moduleProperty(input, "usePrecompiledHeader", tag);
    if (usePch) {
        if (pchOutput) {
            // create PCH
            args.push("/Yc");
            args.push("/Fp" + FileInfo.toWindowsSeparators(pchOutput.filePath));
            args.push("/Fo" + FileInfo.toWindowsSeparators(objOutput.filePath));
            args.push(FileInfo.toWindowsSeparators(input.filePath));
            var copyCmd = new JavaScriptCommand();
            copyCmd.tag = tag;
            copyCmd.silent = true;
            copyCmd.sourceCode = function() {
                File.copy(input.filePath, outputs[tag + "_pch_copy"][0].filePath);
            };
            commands.push(copyCmd);
        } else {
            // use PCH
            var pchHeaderFilePath = ".obj/" + product.name + '_' + tag + '.pch_copy';
            var pchFilePath = FileInfo.toWindowsSeparators(product.buildDirectory
                + "\\.obj\\" + product.name + "_" + tag + ".pch");
            args.push("/FI" + pchHeaderFilePath);
            args.push("/Yu" + pchHeaderFilePath);
            args.push("/Fp" + pchFilePath);
        }
    }

    args = args.concat(ModUtils.moduleProperty(input, 'platformFlags'),
                       ModUtils.moduleProperty(input, 'flags'),
                       ModUtils.moduleProperty(input, 'platformFlags', tag),
                       ModUtils.moduleProperty(input, 'flags', tag));

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
    cmd.workingDirectory = product.buildDirectory + "/.obj";
    cmd.responseFileUsagePrefix = '@';
    // cl.exe outputs the cpp file name. We filter that out.
    cmd.inputFileName = input.fileName;
    cmd.stdoutFilterFunction = function(output) {
        return output.split(inputFileName + "\r\n").join("");
    };
    commands.push(cmd);
    return commands;
}

function collectLibraryDependencies(product) {
    var seen = {};
    var result = [];

    function addFilePath(filePath) {
        result.push(filePath);
    }

    function addArtifactFilePaths(dep, artifacts) {
        if (!artifacts)
            return;
        var artifactFilePaths = artifacts.map(function(a) { return a.filePath; });
        artifactFilePaths.forEach(addFilePath);
    }

    function addExternalLibs(obj) {
        var externalLibs = [].concat(
                    ModUtils.sanitizedModuleProperty(obj, "cpp", "staticLibraries"),
                    ModUtils.sanitizedModuleProperty(obj, "cpp", "dynamicLibraries"));
        externalLibs.forEach(function (libName) {
            if (!libName.match(/\.lib$/i))
                libName += ".lib";
            addFilePath(libName);
        });
    }

    function traverse(dep) {
        if (seen.hasOwnProperty(dep.name))
            return;
        seen[dep.name] = true;
        var staticLibraryArtifacts = dep.artifacts["staticlibrary"];
        var dynamicLibraryArtifacts = staticLibraryArtifacts
                ? null : dep.artifacts["dynamiclibrary_import"];
        if (staticLibraryArtifacts) {
            dep.dependencies.forEach(traverse);
            addArtifactFilePaths(dep, staticLibraryArtifacts);
            addExternalLibs(dep);
        } else if (dynamicLibraryArtifacts) {
            addArtifactFilePaths(dep, dynamicLibraryArtifacts);
        }
    }

    product.dependencies.forEach(traverse);
    addExternalLibs(product);
    return result;
}

function prepareLinker(project, product, inputs, outputs, input, output) {
    var i;
    var linkDLL = (outputs.dynamiclibrary ? true : false)
    var primaryOutput = (linkDLL ? outputs.dynamiclibrary[0] : outputs.application[0])
    var debugInformation = product.cpp.debugInformation;
    var generateManifestFiles = !linkDLL && product.cpp.generateManifestFile;

    var args = ['/nologo']
    if (linkDLL) {
        args.push('/DLL');
        args.push('/IMPLIB:' + FileInfo.toWindowsSeparators(outputs.dynamiclibrary_import[0].filePath));
    }

    if (debugInformation) {
        args.push("/DEBUG");
        var debugInfo = outputs.debuginfo_app || outputs.debuginfo_dll;
        if (debugInfo)
            args.push("/PDB:" + debugInfo[0].fileName);
    } else {
        args.push('/INCREMENTAL:NO')
    }

    switch (product.qbs.architecture) {
    case "x86":
        args.push("/MACHINE:X86");
        break;
    case "x86_64":
        args.push("/MACHINE:X64");
        break;
    case "ia64":
        args.push("/MACHINE:IA64");
        break;
    case "armv7":
        args.push("/MACHINE:ARM");
        break;
    }

    var minimumWindowsVersion = product.cpp.minimumWindowsVersion;
    var subsystemSwitch = undefined;
    if (minimumWindowsVersion || product.consoleApplication !== undefined) {
        // Ensure that we default to console if product.consoleApplication is undefined
        // since that could still be the case if only minimumWindowsVersion had been defined
        subsystemSwitch = product.consoleApplication === false ? '/SUBSYSTEM:WINDOWS' : '/SUBSYSTEM:CONSOLE';
    }

    if (minimumWindowsVersion) {
        var subsystemVersion = WindowsUtils.getWindowsVersionInFormat(minimumWindowsVersion,
                                                                      'subsystem');
        if (subsystemVersion) {
            subsystemSwitch += ',' + subsystemVersion;
            args.push('/OSVERSION:' + subsystemVersion);
        }
    }

    if (subsystemSwitch)
        args.push(subsystemSwitch);

    var linkerOutputNativeFilePath;
    var manifestFileName;
    if (generateManifestFiles) {
        linkerOutputNativeFilePath
                = FileInfo.toWindowsSeparators(
                    FileInfo.path(primaryOutput.filePath) + "/intermediate."
                        + primaryOutput.fileName);
        manifestFileName = linkerOutputNativeFilePath + ".manifest";
        args.push('/MANIFEST', '/MANIFESTFILE:' + manifestFileName)
    } else {
        linkerOutputNativeFilePath = FileInfo.toWindowsSeparators(primaryOutput.filePath);
    }

    var allInputs = inputs.obj || [];
    for (i in allInputs) {
        var fileName = FileInfo.toWindowsSeparators(allInputs[i].filePath)
        args.push(fileName)
    }

    function toWindowsSeparator(filePath) { return FileInfo.toWindowsSeparators(filePath); }
    args = args.concat(collectLibraryDependencies(product).map(toWindowsSeparator));

    if (product.cpp.entryPoint)
        args.push("/ENTRY:" + product.cpp.entryPoint);

    args.push('/OUT:' + linkerOutputNativeFilePath)
    var libraryPaths = product.cpp.libraryPaths;
    if (libraryPaths)
        libraryPaths = [].uniqueConcat(libraryPaths);
    for (i in libraryPaths) {
        args.push('/LIBPATH:' + FileInfo.toWindowsSeparators(libraryPaths[i]))
    }
    var linkerFlags = product.cpp.platformLinkerFlags.concat(product.cpp.linkerFlags);
    args = args.concat(linkerFlags);
    if (product.cpp.allowUnresolvedSymbols)
        args.push("/FORCE:UNRESOLVED");

    var linkerPath = product.cpp.linkerPath;
    var wrapperArgs = product.cpp.linkerWrapper;
    if (wrapperArgs && wrapperArgs.length > 0) {
        args.unshift(linkerPath);
        linkerPath = wrapperArgs.shift();
        args = wrapperArgs.concat(args);
    }
    var commands = [];
    var cmd = new Command(linkerPath, args)
    cmd.description = 'linking ' + primaryOutput.fileName;
    cmd.highlight = 'linker';
    cmd.workingDirectory = FileInfo.path(primaryOutput.filePath)
    cmd.responseFileUsagePrefix = '@';
    cmd.stdoutFilterFunction = function(output) {
        return output.replace(/^ +Creating library.*\r\n$/, "");
    };
    commands.push(cmd);

    if (generateManifestFiles) {
        var outputNativeFilePath = FileInfo.toWindowsSeparators(primaryOutput.filePath);
        cmd = new JavaScriptCommand();
        cmd.src = linkerOutputNativeFilePath;
        cmd.dst = outputNativeFilePath;
        cmd.sourceCode = function() {
            File.copy(src, dst);
        }
        cmd.silent = true
        commands.push(cmd);
        args = [
            '/nologo', '/manifest', manifestFileName,
            "/outputresource:" + outputNativeFilePath + ";#" + (linkDLL ? "2" : "1")
        ]
        cmd = new Command("mt.exe", args)
        cmd.description = 'embedding manifest into ' + primaryOutput.fileName;
        cmd.highlight = 'linker';
        cmd.workingDirectory = FileInfo.path(primaryOutput.filePath)
        commands.push(cmd);
    }

    return commands;
}

