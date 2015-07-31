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

var FileInfo = loadExtension("qbs.FileInfo");
var ModUtils = loadExtension("qbs.ModUtils");
var WindowsUtils = loadExtension("qbs.WindowsUtils");

function prepareCompiler(project, product, inputs, outputs, input, output) {
    var i;
    var optimization = ModUtils.moduleProperty(input, "optimization")
    var debugInformation = ModUtils.moduleProperty(input, "debugInformation")
    var args = ['/nologo', '/c']

    // Determine which C-language we're compiling
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(Object.keys(outputs)));
    if (!["c", "cpp"].contains(tag))
        throw ("unsupported source language");

    // Whether we're compiling a precompiled header or normal source file
    var pchOutput = outputs[tag + "_pch"] ? outputs[tag + "_pch"][0] : undefined;

    // enable unwind semantics
    args.push("/EHsc")
    // optimization:
    if (optimization === 'small')
        args.push('/Os')
    else if (optimization === 'fast')
        args.push('/O2')

    if (debugInformation) {
        if (ModUtils.moduleProperty(product, "separateDebugInformation"))
            args.push('/Zi');
        else
            args.push('/Z7');
    }

    var rtl = ModUtils.moduleProperty(product, "runtimeLibrary");
    if (rtl) {
        rtl = (rtl === "static" ? "/MT" : "/MD");
        if (product.moduleProperty("qbs", "enableDebugCode"))
            rtl += "d";
        args.push(rtl);
    }

    // warnings:
    var warningLevel = ModUtils.moduleProperty(input, "warningLevel")
    if (warningLevel === 'none')
        args.push('/w')
    if (warningLevel === 'all')
        args.push('/Wall')
    if (ModUtils.moduleProperty(input, "treatWarningsAsErrors"))
        args.push('/WX')
    var includePaths = ModUtils.moduleProperties(input, 'includePaths');
    for (i in includePaths)
        args.push('/I' + FileInfo.toWindowsSeparators(includePaths[i]))
    var systemIncludePaths = ModUtils.moduleProperties(input, 'systemIncludePaths');
    for (i in systemIncludePaths)
        args.push('/I' + FileInfo.toWindowsSeparators(systemIncludePaths[i]))
    var platformDefines = ModUtils.moduleProperty(input, 'platformDefines');
    for (i in platformDefines)
        args.push('/D' + platformDefines[i])
    var defines = ModUtils.moduleProperties(input, 'defines');
    for (i in defines)
        args.push('/D' + defines[i])

    var minimumWindowsVersion = ModUtils.moduleProperty(product, "minimumWindowsVersion");
    if (minimumWindowsVersion) {
        var hexVersion = WindowsUtils.getWindowsVersionInFormat(minimumWindowsVersion, 'hex');
        if (hexVersion) {
            var versionDefs = [ 'WINVER', '_WIN32_WINNT', '_WIN32_WINDOWS' ];
            for (i in versionDefs) {
                args.push('/D' + versionDefs[i] + '=' + hexVersion);
            }
        } else {
            print('WARNING: Unknown Windows version "' + minimumWindowsVersion + '"');
        }
    }

    var objOutput = outputs.obj ? outputs.obj[0] : undefined
    args.push('/Fo' + FileInfo.toWindowsSeparators(objOutput.filePath))
    args.push(FileInfo.toWindowsSeparators(input.filePath))

    var prefixHeaders = ModUtils.moduleProperty(product, "prefixHeaders");
    for (i in prefixHeaders)
        args.push("/FI" + FileInfo.toWindowsSeparators(prefixHeaders[i]));

    // Language
    if (tag === "cpp")
        args.push("/TP");
    else if (tag === "c")
        args.push("/TC");

    // precompiled header file
    var pch = ModUtils.moduleProperty(product, "precompiledHeader", tag);
    if (pch) {
        if (pchOutput) {
            // create PCH
            args.push("/Yc");
            args.push("/Fp" + FileInfo.toWindowsSeparators(pchOutput.filePath));
            args.push("/Fo" + FileInfo.toWindowsSeparators(objOutput.filePath));
            args.push(FileInfo.toWindowsSeparators(input.filePath));
        } else {
            // use PCH
            var pchHeaderName = FileInfo.toWindowsSeparators(pch);
            var pchName = FileInfo.toWindowsSeparators(ModUtils.moduleProperty(product, "precompiledHeaderDir")
                + "\\.obj\\" + product.name + "_" + tag + ".pch");
            args.push("/FI" + pchHeaderName);
            args.push("/Yu" + pchHeaderName);
            args.push("/Fp" + pchName);
        }
    }

    args = args.concat(ModUtils.moduleProperties(input, 'platformFlags'),
                       ModUtils.moduleProperties(input, 'flags'),
                       ModUtils.moduleProperties(input, 'platformFlags', tag),
                       ModUtils.moduleProperties(input, 'flags', tag));

    var compilerPath = ModUtils.moduleProperty(product, "compilerPath");
    var wrapperArgs = ModUtils.moduleProperty(product, "compilerWrapper");
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
    return cmd;
}

function prepareLinker(project, product, inputs, outputs, input, output) {
    var i;
    var linkDLL = (outputs.dynamiclibrary ? true : false)
    var primaryOutput = (linkDLL ? outputs.dynamiclibrary[0] : outputs.application[0])
    var debugInformation = ModUtils.moduleProperty(product, "debugInformation")
    var generateManifestFiles = !linkDLL && ModUtils.moduleProperty(product, "generateManifestFiles")

    var args = ['/nologo']
    if (linkDLL) {
        args.push('/DLL');
        args.push('/IMPLIB:' + FileInfo.toWindowsSeparators(outputs.dynamiclibrary_import[0].filePath));
    }

    if (debugInformation) {
        args.push("/DEBUG");
        if (outputs.debuginfo)
            args.push("/PDB:" + outputs.debuginfo[0].fileName);
    } else {
        args.push('/INCREMENTAL:NO')
    }

    var minimumWindowsVersion = ModUtils.moduleProperty(product, "minimumWindowsVersion");
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
        } else {
            print('WARNING: Unknown Windows version "' + minimumWindowsVersion + '"');
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

    var allInputs = (inputs.obj || []).concat(inputs.staticlibrary || [])
    if (inputs.dynamiclibrary_import)
        allInputs = allInputs.concat(inputs.dynamiclibrary_import);
    for (i in allInputs) {
        var fileName = FileInfo.toWindowsSeparators(allInputs[i].filePath)
        args.push(fileName)
    }

    function pushLibs(libs)
    {
        if (!libs)
            return;
        var s = {};
        var c = libs.length;
        for (var i = 0; i < c; ++i) {
            var lib = FileInfo.toWindowsSeparators(libs[i]);
            if (!lib.match(/\.lib$/i))
                lib += ".lib";
            if (s[lib])
                continue;
            s[lib] = true;
            args.push(lib);
        }
    }

    pushLibs(ModUtils.modulePropertiesFromArtifacts(product, inputs.staticlibrary,
                                                    "cpp", "staticLibraries"));
    pushLibs(ModUtils.moduleProperties(product, "dynamicLibraries"));

    if (product.moduleProperty("cpp", "entryPoint"))
        args.push("/ENTRY:" + product.moduleProperty("cpp", "entryPoint"));

    args.push('/OUT:' + linkerOutputNativeFilePath)
    var libraryPaths = ModUtils.moduleProperties(product, 'libraryPaths');
    for (i in libraryPaths) {
        args.push('/LIBPATH:' + FileInfo.toWindowsSeparators(libraryPaths[i]))
    }
    var linkerFlags = ModUtils.moduleProperties(product, 'platformLinkerFlags').concat(
                ModUtils.moduleProperties(product, 'linkerFlags'));
    args = args.concat(linkerFlags);
    if (ModUtils.moduleProperty(product, "allowUnresolvedSymbols"))
        args.push("/FORCE:UNRESOLVED");

    var commands = [];
    var cmd = new Command(product.moduleProperty("cpp", "linkerPath"), args)
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

