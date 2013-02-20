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

    var arch = ModUtils.findFirst(config, "architecture")
    if (arch === 'x86_64')
        args.push('-m64');
    else if (arch === 'x86')
        args.push('-m32');

    if (ModUtils.findFirst/config, "debugInformation")
        args.push('-g');
    var opt = ModUtils.findFirst(config, "optimization")
    if (opt === 'fast')
        args.push('-O2');
    if (opt === 'small')
        args.push('-Os');

    var warnings = ModUtils.findFirst(config, "warningLevel")
    if (warnings === 'none')
        args.push('-w');
    if (warnings === 'all') {
        args.push('-Wall');
        args.push('-Wextra');
    }
    if (ModUtils.findFirst(config, "treatWarningsAsErrors"))
        args.push('-Werror');

    return args;
}

function removePrefixAndSuffix(str, prefix, suffix)
{
    return str.substr(prefix.length, str.length - prefix.length - suffix.length);
}

// ### what we actually need here is something like product.usedFileTags
//     that contains all fileTags that have been used when applying the rules.
function additionalFlags(product, includePaths, frameworkPaths, systemIncludePaths, fileName, output)
{
    var args = []
    if (product.type.indexOf('staticlibrary') >= 0 || product.type.indexOf('dynamiclibrary') >= 0) {
        if (product.moduleProperty("qbs", "toolchain") !== "mingw")
            args.push('-fPIC');
    } else if (product.type.indexOf('application') >= 0) {
        var positionIndependentCode = product.moduleProperty('cpp', 'positionIndependentCode')
        if (positionIndependentCode && product.moduleProperty("qbs", "toolchain") !== "mingw")
            args.push('-fPIE');
    } else {
        throw ("The product's type must be in {staticlibrary, dynamiclibrary, application}. But it is " + product.type + '.');
    }
    var sysroot = ModUtils.findFirst(product, "sysroot")
    if (sysroot)
        args.push('--sysroot=' + sysroot)
    var i;
    var cppFlags = ModUtils.appendAll(input, 'cppFlags');
    for (i in cppFlags)
        args.push('-Wp,' + cppFlags[i])
    var platformDefines = ModUtils.appendAll(input, 'platformDefines');
    for (i in platformDefines)
        args.push('-D' + platformDefines[i]);
    var defines = ModUtils.appendAll(input, 'defines');
    for (i in defines)
        args.push('-D' + defines[i]);
    for (i in includePaths)
        args.push('-I' + includePaths[i]);
    for (i in frameworkPaths)
        args.push('-F' + frameworkPaths[i]);
    for (i in systemIncludePaths)
        args.push('-isystem' + systemIncludePaths[i]);
    args.push('-c');
    args.push(fileName);
    args.push('-o');
    args.push(output.fileName);
    return args
}
