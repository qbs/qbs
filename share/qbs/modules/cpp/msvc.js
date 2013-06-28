function prepareCompiler(product, input, outputs, platformDefines, defines, includePaths, systemIncludePaths, cFlags, cxxFlags)
{
    var i;
    var optimization = ModUtils.moduleProperty(input, "optimization")
    var debugInformation = ModUtils.moduleProperty(input, "debugInformation")
    var args = ['/nologo', '/c']

    // C or C++
    var isCxx = true;
    if (input.fileTags.contains('c')) {
        isCxx = false;
        args.push('/TC')
    }

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
    args.push('/Fo' + FileInfo.toWindowsSeparators(objOutput.fileName))
    args.push(FileInfo.toWindowsSeparators(input.fileName))

    var prefixHeaders = ModUtils.moduleProperty(product, "prefixHeaders");
    for (i in prefixHeaders)
        args.push("/FI" + FileInfo.toWindowsSeparators(prefixHeaders[i]));

    if (isCxx) {
        // precompiled header file
        var pch = ModUtils.moduleProperty(product, "precompiledHeader")
        if (pch) {
            var pchOutput = outputs["c++_pch"] ? outputs["c++_pch"][0] : undefined
            if (pchOutput) {
                // create pch
                args.push('/Yc')
                args.push('/Fp' + FileInfo.toWindowsSeparators(pchOutput.fileName))
                args.push('/Fo' + FileInfo.toWindowsSeparators(objOutput.fileName))
                args.push('/TP')
                args.push(FileInfo.toWindowsSeparators(input.fileName))
            } else {
                // use pch
                var pchHeaderName = FileInfo.toWindowsSeparators(pch)
                var pchName = FileInfo.toWindowsSeparators(ModUtils.moduleProperty(product, "precompiledHeaderDir")
                        + "\\.obj\\" + product.name + "\\" + product.name + ".pch")
                args.push("/FI" + pchHeaderName)
                args.push("/Yu" + pchHeaderName)
                args.push("/Fp" + pchName)
            }
        }
        if (cxxFlags)
            args = args.concat(cxxFlags);
    } else {
        if (cFlags)
            args = args.concat(cFlags);
    }

    var compilerPath = ModUtils.moduleProperty(product, "compilerPath");
    var wrapperArgs = ModUtils.moduleProperty(product, "compilerWrapper");
    if (wrapperArgs && wrapperArgs.length > 0) {
        args.unshift(compilerPath);
        compilerPath = wrapperArgs.shift();
        args = wrapperArgs.concat(args);
    }
    var cmd = new Command(compilerPath, args)
    cmd.description = (pchOutput ? 'pre' : '') + 'compiling ' + FileInfo.fileName(input.fileName)
    cmd.highlight = "compiler";
    cmd.workingDirectory = FileInfo.path(objOutput.fileName)
    cmd.responseFileUsagePrefix = '@';
    // cl.exe outputs the cpp file name. We filter that out.
    cmd.stdoutFilterFunction = "function(output) {";
    cmd.stdoutFilterFunction += "return output.replace('" + FileInfo.fileName(input.fileName) + "\\r\\n', '');";
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
        args.push('/IMPLIB:' + FileInfo.toWindowsSeparators(outputs.dynamiclibrary_import[0].fileName));
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
                    FileInfo.path(primaryOutput.fileName) + "/intermediate."
                        + FileInfo.fileName(primaryOutput.fileName));
        manifestFileName = linkerOutputNativeFilePath + ".manifest";
        args.push('/MANIFEST', '/MANIFESTFILE:' + manifestFileName)
    } else {
        linkerOutputNativeFilePath = FileInfo.toWindowsSeparators(primaryOutput.fileName);
    }

    var allInputs = inputs.obj.concat(inputs.staticlibrary || [])
    if (inputs.dynamiclibrary_import)
        allInputs = allInputs.concat(inputs.dynamiclibrary_import);
    for (i in allInputs) {
        var fileName = FileInfo.toWindowsSeparators(allInputs[i].fileName)
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

    var commands = [];
    var cmd = new Command("link.exe", args)
    cmd.description = 'linking ' + FileInfo.fileName(primaryOutput.fileName)
    cmd.highlight = 'linker';
    cmd.workingDirectory = FileInfo.path(primaryOutput.fileName)
    cmd.responseFileUsagePrefix = '@';
    cmd.stdoutFilterFunction =
            function(output)
            {
                return output.replace(/^ +Creating library.*\r\n$/, "");
            };
    commands.push(cmd);

    if (generateManifestFiles) {
        var outputNativeFilePath = FileInfo.toWindowsSeparators(primaryOutput.fileName);
        cmd = new JavaScriptCommand();
        cmd.src = linkerOutputNativeFilePath;
        cmd.dst = outputNativeFilePath;
        cmd.sourceCode = function() {
            File.copy(src, dst);
        }
        commands.push(cmd);
        args = [
            '/nologo', '/manifest', manifestFileName,
            "/outputresource:" + outputNativeFilePath + ";#" + (linkDLL ? "2" : "1")
        ]
        cmd = new Command("mt.exe", args)
        cmd.description = 'embedding manifest into ' + FileInfo.fileName(primaryOutput.fileName)
        cmd.highlight = 'linker';
        cmd.workingDirectory = FileInfo.path(primaryOutput.fileName)
        commands.push(cmd);
    }

    return commands;
}

