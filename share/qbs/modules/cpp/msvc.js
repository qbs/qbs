function prepareCompiler(product, input, outputs, defines, includePaths, compilerFlags)
{
    var optimization = input.module.optimization
    var debugInformation = input.module.debugInformation
    var architecture = input.module.architecture
    var toolchainInstallPath = product.module.toolchainInstallPath

    var args = ['/nologo', '/c', '/Zm200', '/Zc:wchar_t-']

    // C or C++
    if (input.fileTags.indexOf('c') >= 0) {
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
    var warningLevel = input.module.warningLevel
    if (warningLevel === 'none')
        args.push('/w')
    if (warningLevel === 'all')
        args.push('/Wall')
    for (var i in includePaths)
        args.push('/I' + FileInfo.toWindowsSeparators(includePaths[i]))
    for (i in defines)
        args.push('/D' + defines[i])

    var objOutput = outputs.obj ? outputs.obj[0] : undefined
    var pchOutput = outputs["c++_pch"] ? outputs["c++_pch"][0] : undefined

    // precompiled header file
    if (product.module.precompiledHeader) {
        if (pchOutput) {
            // create pch
            args.push('/Yc')
            args.push('/Fp' + FileInfo.toWindowsSeparators(pchOutput.fileName))
            args.push('/Fo' + FileInfo.toWindowsSeparators(objOutput.fileName))
            args.push('/TP')
            args.push(FileInfo.toWindowsSeparators(input.fileName))
        } else {
            // use pch
            var pchHeaderName = FileInfo.toWindowsSeparators(product.module.precompiledHeader)
            var pchName = FileInfo.toWindowsSeparators(product.module.precompiledHeaderDir[0] + "\\.obj\\" + product.name + "\\" + product.name + ".pch")
            args.push("/FI" + pchHeaderName)
            args.push("/Yu" + pchHeaderName)
            args.push("/Fp" + pchName)
        }
    }
    args.push('/Fo' + FileInfo.toWindowsSeparators(objOutput.fileName))
    args.push(FileInfo.toWindowsSeparators(input.fileName))

    var clPath = toolchainInstallPath + '/VC/bin/'
    var is64bit = (architecture === "x86_64")
    if (is64bit)
        clPath += 'amd64/'
    clPath += 'cl.exe'

    for (var i in compilerFlags) {
        args.push(i)
    }

    var cmd = new Command(clPath, args)
    cmd.description = (pchOutput ? 'pre' : '') + 'compiling ' + FileInfo.fileName(input.fileName)
    cmd.highlight = "compiler";
    cmd.workingDirectory = FileInfo.path(objOutput.fileName)
    cmd.responseFileThreshold = product.module.responseFileThreshold
    cmd.responseFileUsagePrefix = '@';
    // cl.exe outputs the cpp file name. We filter that out.
    cmd.stdoutFilterFunction = "function(output) {";
    cmd.stdoutFilterFunction += "return output.replace('" + FileInfo.fileName(input.fileName) + "\\r\\n', '');";
    cmd.stdoutFilterFunction += "}";
    return cmd;
}

function prepareLinker(product, inputs, outputs, libraryPaths, dynamicLibraries, staticLibraries)
{
    var linkDLL = (outputs.dynamiclibrary ? true : false)
    var primaryOutput = (linkDLL ? outputs.dynamiclibrary[0] : outputs.application[0])
    var optimization = product.module.optimization
    var debugInformation = product.module.debugInformation
    var architecture = product.module.architecture
    var windowsSDKPath = product.module.windowsSDKPath
    var generateManifestFiles = !linkDLL && product.module.generateManifestFiles
    var toolchainInstallPath = product.module.toolchainInstallPath

    var args = ['/nologo']
    if (linkDLL) {
        args.push('/DLL');
        args.push('/IMPLIB:' + FileInfo.toWindowsSeparators(outputs.dynamiclibrary_import[0].fileName));
    }

    if (debugInformation)
        args.push('/DEBUG')
    else
        args.push('/INCREMENTAL:NO')

    var manifestFileName
    if (generateManifestFiles) {
        manifestFileName = FileInfo.toWindowsSeparators(primaryOutput.fileName)
        manifestFileName += '.intermediate.manifest'
        args.push('/MANIFEST', '/MANIFESTFILE:' + manifestFileName)
    }

    var allInputs = inputs.obj.concat(inputs.staticlibrary || [])
    if (inputs.dynamiclibrary_import)
        allInputs = inputs.obj.concat(inputs.dynamiclibrary_import)
    for (var i in allInputs) {
        var fileName = FileInfo.toWindowsSeparators(allInputs[i].fileName)
        args.push(fileName)
    }
    for (var i in staticLibraries) {
        var staticLibrary = staticLibraries[i];
        if (!staticLibrary.match(/\.lib$/i))
            staticLibrary += ".lib";
        args.push(staticLibrary)
    }
    for (var i in dynamicLibraries) {
        var dynamicLibrary = dynamicLibraries[i];
        if (!dynamicLibrary.match(/\.lib$/i))
            dynamicLibrary += ".lib";
        args.push(dynamicLibrary)
    }

    var nativeOutputFileName = FileInfo.toWindowsSeparators(primaryOutput.fileName)
    args.push('/OUT:' + nativeOutputFileName)
    for (var i in libraryPaths) {
        args.push('/LIBPATH:' + FileInfo.toWindowsSeparators(libraryPaths[i]))
    }
    args = args.concat(dynamicLibraries)
    var is64bit = (architecture == "x86_64")

    var linkerPath = toolchainInstallPath + '/VC/bin/'
    if (is64bit)
        linkerPath += 'amd64/'
    linkerPath += 'link.exe'

    var commands = [];
    var cmd = new Command(linkerPath, args)
    cmd.description = 'linking ' + FileInfo.fileName(primaryOutput.fileName)
    cmd.highlight = 'linker';
    cmd.workingDirectory = FileInfo.path(primaryOutput.fileName)
    cmd.responseFileThreshold = product.module.responseFileThreshold
    cmd.responseFileUsagePrefix = '@';
    commands.push(cmd);

    if (generateManifestFiles) {
        // embed the generated manifest files
        var args = [
            '/nologo', '/manifest', manifestFileName,
            '/outputresource:' + nativeOutputFileName + ';1'
        ]
        var mtPath = windowsSDKPath + '/bin/mt.exe'
        var cmd = new Command(mtPath, args)
        cmd.description = 'embedding manifest into ' + FileInfo.fileName(primaryOutput.fileName)
        cmd.highlight = 'linker';
        cmd.workingDirectory = FileInfo.path(primaryOutput.fileName)
        commands.push(cmd);
    }

    return commands;
}

