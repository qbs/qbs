var Windows = loadFile("windows.js");

function prepareCompiler(product, input, outputs, platformDefines, defines, includePaths, systemIncludePaths, cFlags, cxxFlags)
{
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
        args.push('/Zi')
        args.push('/MDd')
    } else {
        args.push('/MD')
    }
    // warnings:
    var warningLevel = ModUtils.moduleProperty(input, "warningLevel")
    if (warningLevel === 'none')
        args.push('/w')
    if (warningLevel === 'all')
        args.push('/Wall')
    if (ModUtils.moduleProperty(input, "treatWarningsAsErrors"))
        args.push('/WX')
    for (i in includePaths)
        args.push('/I' + FileInfo.toWindowsSeparators(includePaths[i]))
    for (i in systemIncludePaths)
        args.push('/I' + FileInfo.toWindowsSeparators(systemIncludePaths[i]))
    for (i in platformDefines)
        args.push('/D' + platformDefines[i])
    for (i in defines)
        args.push('/D' + defines[i])

    var minimumWindowsVersion = ModUtils.moduleProperty(product, "minimumWindowsVersion");
    if (minimumWindowsVersion) {
        var hexVersion = Windows.getWindowsVersionInFormat(minimumWindowsVersion, 'hex');
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
                + "\\.obj\\" + product.name + "\\" + product.name + "_" + tag + ".pch");
            args.push("/FI" + pchHeaderName);
            args.push("/Yu" + pchHeaderName);
            args.push("/Fp" + pchName);
        }
    }

    if (cxxFlags && tag === "cpp")
        args = args.concat(cxxFlags);
    else if (cFlags && tag === "c")
        args = args.concat(cFlags);

    var compilerPath = ModUtils.moduleProperty(product, "compilerPath");
    var wrapperArgs = ModUtils.moduleProperty(product, "compilerWrapper");
    if (wrapperArgs && wrapperArgs.length > 0) {
        args.unshift(compilerPath);
        compilerPath = wrapperArgs.shift();
        args = wrapperArgs.concat(args);
    }
    var cmd = new Command(compilerPath, args)
    cmd.description = (pchOutput ? 'pre' : '') + 'compiling ' + FileInfo.fileName(input.filePath);
    if (pchOutput)
        cmd.description += ' (' + tag + ')';
    cmd.highlight = "compiler";
    cmd.workingDirectory = product.buildDirectory + "/.obj/" + product.name;
    cmd.responseFileUsagePrefix = '@';
    // cl.exe outputs the cpp file name. We filter that out.
    cmd.stdoutFilterFunction = "function(output) {";
    cmd.stdoutFilterFunction += "return output.replace(/"
            + FileInfo.fileName(input.filePath) + "\\r\\n/g, '');";
    cmd.stdoutFilterFunction += "}";
    return cmd;
}

function prepareLinker(product, inputs, outputs, libraryPaths, dynamicLibraries, staticLibraries, linkerFlags)
{
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

    if (debugInformation)
        args.push('/DEBUG')
    else
        args.push('/INCREMENTAL:NO')

    var minimumWindowsVersion = ModUtils.moduleProperty(product, "minimumWindowsVersion");
    var subsystemSwitch = undefined;
    if (minimumWindowsVersion || product.consoleApplication !== undefined) {
        // Ensure that we default to console if product.consoleApplication is undefined
        // since that could still be the case if only minimumWindowsVersion had been defined
        subsystemSwitch = product.consoleApplication === false ? '/SUBSYSTEM:WINDOWS' : '/SUBSYSTEM:CONSOLE';
    }

    if (minimumWindowsVersion) {
        var subsystemVersion = Windows.getWindowsVersionInFormat(minimumWindowsVersion, 'subsystem');
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
                        + FileInfo.fileName(primaryOutput.filePath));
        manifestFileName = linkerOutputNativeFilePath + ".manifest";
        args.push('/MANIFEST', '/MANIFESTFILE:' + manifestFileName)
    } else {
        linkerOutputNativeFilePath = FileInfo.toWindowsSeparators(primaryOutput.filePath);
    }

    var allInputs = inputs.obj.concat(inputs.staticlibrary || [])
    if (inputs.dynamiclibrary_import)
        allInputs = allInputs.concat(inputs.dynamiclibrary_import);
    for (i in allInputs) {
        var fileName = FileInfo.toWindowsSeparators(allInputs[i].filePath)
        args.push(fileName)
    }
    for (i in staticLibraries) {
        var staticLibrary = staticLibraries[i];
        if (!staticLibrary.match(/\.lib$/i))
            staticLibrary += ".lib";
        args.push(staticLibrary)
    }
    for (i in dynamicLibraries) {
        var dynamicLibrary = dynamicLibraries[i];
        if (!dynamicLibrary.match(/\.lib$/i))
            dynamicLibrary += ".lib";
        args.push(dynamicLibrary)
    }

    args.push('/OUT:' + linkerOutputNativeFilePath)
    for (i in libraryPaths) {
        args.push('/LIBPATH:' + FileInfo.toWindowsSeparators(libraryPaths[i]))
    }
    args = args.concat(linkerFlags);
    if (ModUtils.moduleProperty(product, "allowUnresolvedSymbols"))
        args.push("/FORCE:UNRESOLVED");

    var commands = [];
    var cmd = new Command(product.moduleProperty("cpp", "linkerPath"), args)
    cmd.description = 'linking ' + FileInfo.fileName(primaryOutput.filePath)
    cmd.highlight = 'linker';
    cmd.workingDirectory = FileInfo.path(primaryOutput.filePath)
    cmd.responseFileUsagePrefix = '@';
    cmd.stdoutFilterFunction =
            function(output)
            {
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
        cmd.description = 'embedding manifest into ' + FileInfo.fileName(primaryOutput.filePath)
        cmd.highlight = 'linker';
        cmd.workingDirectory = FileInfo.path(primaryOutput.filePath)
        commands.push(cmd);
    }

    return commands;
}

