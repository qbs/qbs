function libs(libraryPaths, frameworkPaths, rpaths, dynamicLibraries, staticLibraries, frameworks)
{
    var i;
    var args = [];
    if (rpaths && rpaths.length)
        args.push('-Wl,-rpath,' + rpaths.join(':'));
    for (i in libraryPaths) {
        args.push('-L' + libraryPaths[i]);
    }
    for (i in staticLibraries) {
        if (FileInfo.isAbsolutePath(staticLibraries[i])) {
            args.push(staticLibraries[i]);
        } else {
            args.push('-l' + staticLibraries[i]);
        }
    }
    for (i in dynamicLibraries) {
        if (FileInfo.isAbsolutePath(dynamicLibraries[i])) {
            args.push(dynamicLibraries[i]);
        } else {
            args.push('-l' + dynamicLibraries[i]);
        }
    }
    if (frameworkPaths)
        args = args.concat(frameworkPaths.map(function(path) { return '-F' + path }));
    for (i in frameworks)
        args = args.concat(['-framework', frameworks[i]]);
    return args;
}

function configFlags(config) {
    var args = [];
    // architecture
    if (config.module.architecture === 'x86_64')
        args.push('-m64');
    else if (config.module.architecture === 'x86')
        args.push('-m32');
    // optimization:
    if (config.module.debugInformation)
        args.push('-g');
    if (config.module.optimization === 'fast')
        args.push('-O2');
    if (config.module.optimization === 'small')
        args.push('-Os');
    // warnings:
    if (config.module.warningLevel === 'none')
        args.push('-w');
    if (config.module.warningLevel === 'all') {
        args.push('-Wall');
        args.push('-Wextra');
    }
    if (config.module.treatWarningsAsErrors)
        args.push('-Werror');

    return args;
}

function removePrefixAndSuffix(str, prefix, suffix)
{
    return str.substr(prefix.length, str.length - prefix.length - suffix.length);
}

// ### what we actually need here is something like product.usedFileTags
//     that contains all fileTags that have been used when applying the rules.
function additionalFlags(product, includePaths, systemIncludePaths, fileName, output)
{
    var args = []
    if (product.type.indexOf('staticlibrary') >= 0 || product.type.indexOf('dynamiclibrary') >= 0) {
        if (product.modules.qbs.toolchain !== "mingw")
            args.push('-fPIC');
    } else if (product.type.indexOf('application') >= 0) {
        var positionIndependentCode = ModUtils.findFirst(product.modules, 'cpp', 'positionIndependentCode')
        if (positionIndependentCode && product.modules.qbs.toolchain !== "mingw")
            args.push('-fPIE');
    } else {
        throw ("The product's type must be in {staticlibrary, dynamiclibrary, application}. But it is " + product.type + '.');
    }
    if (product.module.sysroot)
        args.push('--sysroot=' + product.module.sysroot)
    var i;
    var cppFlags = ModUtils.appendAll(input, 'cppFlags');
    for (i in cppFlags)
        args.push('-Wp,' + cppFlags[i])
    var platformDefines = ModUtils.appendAll(input, 'platformDefines');
    for (i in platformDefines)
        args.push('-D' + product.module.platformDefines[i]);
    var defines = ModUtils.appendAll(input, 'defines');
    for (i in defines)
        args.push('-D' + defines[i]);
    for (i in includePaths)
        args.push('-I' + includePaths[i]);
    for (i in systemIncludePaths)
        args.push('-isystem' + systemIncludePaths[i]);
    args.push('-c');
    args.push(fileName);
    args.push('-o');
    args.push(output.fileName);
    return args
}
