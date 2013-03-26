function libraryLinkerFlags(libraryPaths, frameworkPaths, systemFrameworkPaths, rpaths, dynamicLibraries, staticLibraries, frameworks, weakFrameworks)
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
    if (systemFrameworkPaths)
        args = args.concat(systemFrameworkPaths.map(function(path) { return '-iframework' + path }));
    for (i in frameworks)
        args = args.concat(['-framework', frameworks[i]]);
    for (i in weakFrameworks)
        args = args.concat(['-weak_framework', weakFrameworks[i]]);
    return args;
}

// for compiler AND linker
function configFlags(config) {
    var args = [];

    var arch = ModUtils.moduleProperty(config, "architecture")
    if (arch === 'x86_64')
        args.push('-m64');
    else if (arch === 'x86')
        args.push('-m32');

    if (ModUtils.moduleProperty(config, "debugInformation"))
        args.push('-g');
    var opt = ModUtils.moduleProperty(config, "optimization")
    if (opt === 'fast')
        args.push('-O2');
    if (opt === 'small')
        args.push('-Os');

    var warnings = ModUtils.moduleProperty(config, "warningLevel")
    if (warnings === 'none')
        args.push('-w');
    if (warnings === 'all') {
        args.push('-Wall');
        args.push('-Wextra');
    }
    if (ModUtils.moduleProperty(config, "treatWarningsAsErrors"))
        args.push('-Werror');

    return args;
}

function removePrefixAndSuffix(str, prefix, suffix)
{
    return str.substr(prefix.length, str.length - prefix.length - suffix.length);
}

// ### what we actually need here is something like product.usedFileTags
//     that contains all fileTags that have been used when applying the rules.
function additionalCompilerFlags(product, includePaths, frameworkPaths, systemIncludePaths, systemFrameworkPaths, fileName, output)
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
    var sysroot = ModUtils.moduleProperty(product, "sysroot")
    if (sysroot) {
        if (product.moduleProperty("qbs", "targetPlatform").indexOf('darwin') !== -1)
            args.push('-isysroot', sysroot);
        else
            args.push('--sysroot=' + sysroot);
    }
    var i;
    var cppFlags = ModUtils.moduleProperties(input, 'cppFlags');
    for (i in cppFlags)
        args.push('-Wp,' + cppFlags[i])
    var platformDefines = ModUtils.moduleProperties(input, 'platformDefines');
    for (i in platformDefines)
        args.push('-D' + platformDefines[i]);
    var defines = ModUtils.moduleProperties(input, 'defines');
    for (i in defines)
        args.push('-D' + defines[i]);
    for (i in includePaths)
        args.push('-I' + includePaths[i]);
    for (i in frameworkPaths)
        args.push('-F' + frameworkPaths[i]);
    for (i in systemIncludePaths)
        args.push('-isystem' + systemIncludePaths[i]);
    for (i in systemFrameworkPaths)
        args.push('-iframework' + systemFrameworkPaths[i]);

    var minimumWindowsVersion = ModUtils.moduleProperty(product, "minimumWindowsVersion");
    if (minimumWindowsVersion && product.moduleProperty("qbs", "targetOS") === "windows") {
        var hexVersion = Windows.getWindowsVersionInFormat(minimumWindowsVersion, 'hex');
        if (hexVersion) {
            var versionDefs = [ 'WINVER', '_WIN32_WINNT', '_WIN32_WINDOWS' ];
            for (i in versionDefs)
                args.push('-D' + versionDefs[i] + '=' + hexVersion);
        } else {
            print('WARNING: Unknown Windows version "' + minimumWindowsVersion + '"');
        }
    }

    args.push('-c');
    args.push(fileName);
    args.push('-o');
    args.push(output.fileName);
    return args
}

function additionalCompilerAndLinkerFlags(product) {
    var args = []

    var minimumMacVersion = ModUtils.moduleProperty(product, "minimumMacVersion");
    if (minimumMacVersion && product.moduleProperty("qbs", "targetOS") === "mac")
        args.push('-mmacosx-version-min=' + minimumMacVersion);

    var minimumiOSVersion = ModUtils.moduleProperty(product, "minimumIosVersion");
    if (minimumiOSVersion && product.moduleProperty("qbs", "targetOS") === "ios") {
        if (product.moduleProperty("qbs", "architecture") === "x86")
            args.push('-mios-simulator-version-min=' + minimumiOSVersion);
        else
            args.push('-miphoneos-version-min=' + minimumiOSVersion);
    }

    return args
}
