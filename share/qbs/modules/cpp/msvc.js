function prepareCompiler(product, input, outputs, platformDefines, defines, includePaths, systemIncludePaths, cFlags, cxxFlags)
{
    var i;
    var optimization = ModUtils.findFirst(input, "optimization")
    var debugInformation = ModUtils.findFirst(input, "debugInformation")
    var architecture = ModUtils.findFirst(input, "architecture")

    var args = ['/nologo', '/c']

    // C or C++
    var isCxx = true;
    if (input.fileTags.indexOf('c') >= 0) {
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
    var warningLevel = ModUtils.findFirst(input, "warningLevel")
    if (warningLevel === 'none')
        args.push('/w')
    if (warningLevel === 'all')
        args.push('/Wall')
    if (ModUtils.findFirst(input, "treatWarningsAsErrors"))
        args.push('/WX')
    for (i in includePaths)
        args.push('/I' + FileInfo.toWindowsSeparators(includePaths[i]))
    for (i in systemIncludePaths)
        args.push('/I' + FileInfo.toWindowsSeparators(systemIncludePaths[i]))
    for (i in platformDefines)
        args.push('/D' + platformDefines[i])
    for (i in defines)
        args.push('/D' + defines[i])

    var objOutput = outputs.obj ? outputs.obj[0] : undefined
    var pchOutput = outputs["c++_pch"] ? outputs["c++_pch"][0] : undefined

    // precompiled header file
    var pch = ModUtils.findFirst(product, "precompiledHeader")
    if (pch) {
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
            var pchName = FileInfo.toWindowsSeparators(ModUtils.findFirst(product, "precompiledHeaderDir")
                    + "\\.obj\\" + product.name + "\\" + product.name + ".pch")
            args.push("/FI" + pchHeaderName)
            args.push("/Yu" + pchHeaderName)
            args.push("/Fp" + pchName)
        }
    }
    args.push('/Fo' + FileInfo.toWindowsSeparators(objOutput.fileName))
    args.push(FileInfo.toWindowsSeparators(input.fileName))

    if (isCxx) {
        if (cxxFlags)
            args = args.concat(cxxFlags);
    } else {
        if (cFlags)
            args = args.concat(cFlags);
    }

    var cmd = new Command("cl.exe", args)
    cmd.description = (pchOutput ? 'pre' : '') + 'compiling ' + FileInfo.fileName(input.fileName)
    cmd.highlight = "compiler";
    cmd.workingDirectory = FileInfo.path(objOutput.fileName)
    cmd.responseFileThreshold = ModUtils.findFirst(product, "responseFileThreshold")
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
    var optimization = ModUtils.findFirst(product, "optimization")
    var debugInformation = ModUtils.findFirst(product, "debugInformation")
    var architecture = ModUtils.findFirst(product, "architecture")
    var windowsSDKPath = ModUtils.findFirst(product, "windowsSDKPath")
    var generateManifestFiles = !linkDLL && ModUtils.findFirst(product, "generateManifestFiles")

    var args = ['/nologo']
    if (linkDLL) {
        args.push('/DLL');
        args.push('/IMPLIB:' + FileInfo.toWindowsSeparators(outputs.dynamiclibrary_import[0].fileName));
    }

    if (debugInformation)
        args.push('/DEBUG')
    else
        args.push('/INCREMENTAL:NO')

    if (product.consoleApplication !== undefined) {
        if (product.consoleApplication)
            args.push('/SUBSYSTEM:CONSOLE');
        else
            args.push('/SUBSYSTEM:WINDOWS');
    }

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
        allInputs = inputs.obj.concat(inputs.dynamiclibrary_import)
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
    var is64bit = (architecture == "x86_64")

    var commands = [];
    var cmd = new Command("link.exe", args)
    cmd.description = 'linking ' + FileInfo.fileName(primaryOutput.fileName)
    cmd.highlight = 'linker';
    cmd.workingDirectory = FileInfo.path(primaryOutput.fileName)
    cmd.responseFileThreshold = ModUtils.findFirst(product, "responseFileThreshold")
    cmd.responseFileUsagePrefix = '@';
    cmd.stdoutFilterFunction =
            function(output)
            {
                return output.replace(/^ +Creating library.*\r\n$/, "");
            };
    commands.push(cmd);

    if (generateManifestFiles) {
        var outputNativeFilePath = FileInfo.toWindowsSeparators(primaryOutput.fileName);
        cmd = new Command("cmd.exe", ["/c", "copy", linkerOutputNativeFilePath,
                                      outputNativeFilePath, ">NUL"]);
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

