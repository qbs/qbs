function libraryLinkerFlags(product, inputs)
{
    var libraryPaths = ModUtils.moduleProperties(product, 'libraryPaths');
    var dynamicLibraries = ModUtils.moduleProperties(product, "dynamicLibraries");
    var staticLibraries = ModUtils.modulePropertiesFromArtifacts(product, inputs.staticlibrary, 'cpp', 'staticLibraries');
    var frameworkPaths = ModUtils.moduleProperties(product, 'frameworkPaths');
    var systemFrameworkPaths = ModUtils.moduleProperties(product, 'systemFrameworkPaths');
    var frameworks = ModUtils.moduleProperties(product, 'frameworks');
    var weakFrameworks = ModUtils.moduleProperties(product, 'weakFrameworks');
    var rpaths = ModUtils.moduleProperties(product, 'rpaths');
    var args = [], i, prefix, suffix, suffixes;

    if (rpaths && rpaths.length)
        args.push('-Wl,-rpath,' + rpaths.join(",-rpath,"));

    // Add filenames of internal library dependencies to the lists
    for (i in inputs.staticlibrary)
        staticLibraries.unshift(inputs.staticlibrary[i].fileName);
    for (i in inputs.dynamiclibrary)
        dynamicLibraries.unshift(inputs.dynamiclibrary[i].fileName);
    for (i in inputs.frameworkbundle)
        frameworks.unshift(inputs.frameworkbundle[i].fileName);

    // Flags for library search paths
    if (libraryPaths)
        args = args.concat(libraryPaths.map(function(path) { return '-L' + path }));
    if (frameworkPaths)
        args = args.concat(frameworkPaths.map(function(path) { return '-F' + path }));
    if (systemFrameworkPaths)
        args = args.concat(systemFrameworkPaths.map(function(path) { return '-iframework' + path }));

    prefix = ModUtils.moduleProperty(product, "staticLibraryPrefix");
    suffixes = ModUtils.moduleProperty(product, "supportedStaticLibrarySuffixes");
    for (i in staticLibraries) {
        if (isLibraryFileName(product, FileInfo.fileName(staticLibraries[i]), prefix, suffixes,
                              false)) {
            args.push(staticLibraries[i]);
        } else {
            args.push('-l' + staticLibraries[i]);
        }
    }

    prefix = ModUtils.moduleProperty(product, "dynamicLibraryPrefix");
    suffix = ModUtils.moduleProperty(product, "dynamicLibrarySuffix");
    for (i in dynamicLibraries) {
        if (isLibraryFileName(product, FileInfo.fileName(dynamicLibraries[i]), prefix, [suffix],
                              true)) {
            args.push(dynamicLibraries[i]);
        } else {
            args.push('-l' + dynamicLibraries[i]);
        }
    }

    suffix = ".framework";
    for (i in frameworks) {
        if (frameworks[i].slice(-suffix.length) === suffix)
            args.push(frameworks[i] + "/" + FileInfo.fileName(frameworks[i]).slice(0, -suffix.length));
        else
            args = args.concat(['-framework', frameworks[i]]);
    }

    for (i in weakFrameworks) {
        if (weakFrameworks[i].slice(-suffix.length) === suffix)
            args = args.concat(['-weak_library', weakFrameworks[i] + "/" + FileInfo.fileName(weakFrameworks[i]).slice(0, -suffix.length)]);
        else
            args = args.concat(['-weak_framework', weakFrameworks[i]]);
    }

    return args;
}

// Returns whether the string looks like a library filename
function isLibraryFileName(product, fileName, prefix, suffixes, isShared)
{
    var suffix, i;
    var os = product.moduleProperty("qbs", "targetOS");
    for (i = 0; i < suffixes.length; ++i) {
        suffix = suffixes[i];
        if (isShared && os.contains("unix") && !os.contains("darwin"))
            suffix += "(\\.[0-9]+){0,3}";
        if (fileName.match("^" + prefix + ".+?\\" + suffix + "$"))
            return true;
    }
    return false;
}

// for compiler AND linker
function configFlags(config) {
    var args = [];

    var arch = ModUtils.moduleProperty(config, "architecture")
    if (config.moduleProperty("qbs", "toolchain").contains("llvm") &&
        config.moduleProperty("qbs", "targetOS").contains("darwin")) {
        args.push("-arch");
        args.push(arch === "x86" ? "i386" : arch);
    } else {
        if (arch === 'x86_64')
            args.push('-m64');
        else if (arch === 'x86')
            args.push('-m32');
    }

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
function additionalCompilerFlags(product, input, output)
{
    var includePaths = ModUtils.moduleProperties(input, 'includePaths');
    var frameworkPaths = ModUtils.moduleProperties(product, 'frameworkPaths');
    var systemIncludePaths = ModUtils.moduleProperties(input, 'systemIncludePaths');
    var systemFrameworkPaths = ModUtils.moduleProperties(input, 'systemFrameworkPaths');

    var EffectiveTypeEnum = { UNKNOWN: 0, LIB: 1, APP: 2 };
    var effectiveType = EffectiveTypeEnum.UNKNOWN;
    var libTypes = {staticlibrary : 1, dynamiclibrary : 1, frameworkbundle : 1};
    var appTypes = {application : 1, applicationbundle : 1};
    var i;
    for (i = product.type.length; --i >= 0;) {
        if (libTypes.hasOwnProperty(product.type[i]) !== -1) {
            effectiveType = EffectiveTypeEnum.LIB;
            break;
        } else if (appTypes.hasOwnProperty(product.type[i]) !== -1) {
            effectiveType = EffectiveTypeEnum.APP;
            break;
        }
    }

    var args = []
    var positionIndependentCode = product.moduleProperty('cpp', 'positionIndependentCode')
    if (effectiveType === EffectiveTypeEnum.LIB) {
        if (positionIndependentCode !== false && !product.moduleProperty("qbs", "toolchain").contains("mingw"))
            args.push('-fPIC');
    } else if (effectiveType === EffectiveTypeEnum.APP) {
        if (positionIndependentCode && !product.moduleProperty("qbs", "toolchain").contains("mingw"))
            args.push('-fPIE');
    } else {
        throw ("The product's type must be in " + JSON.stringify(
                   Object.getOwnPropertyNames(libTypes).concat(Object.getOwnPropertyNames(appTypes)))
                + ". But it is " + JSON.stringify(product.type) + '.');
    }
    var sysroot = ModUtils.moduleProperty(product, "sysroot")
    if (sysroot) {
        if (product.moduleProperty("qbs", "targetOS").contains('darwin'))
            args.push('-isysroot', sysroot);
        else
            args.push('--sysroot=' + sysroot);
    }
    var cppFlags = ModUtils.moduleProperties(input, 'cppFlags');
    for (i in cppFlags)
        args.push('-Wp,' + cppFlags[i])
    var platformDefines = ModUtils.moduleProperty(input, 'platformDefines');
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
    if (minimumWindowsVersion && product.moduleProperty("qbs", "targetOS").contains("windows")) {
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
    args.push(input.fileName);
    args.push('-o');
    args.push(output.fileName);
    return args
}

function additionalCompilerAndLinkerFlags(product) {
    var args = []

    var minimumOsxVersion = ModUtils.moduleProperty(product, "minimumOsxVersion");
    if (minimumOsxVersion && product.moduleProperty("qbs", "targetOS").contains("osx"))
        args.push('-mmacosx-version-min=' + minimumOsxVersion);

    var minimumiOSVersion = ModUtils.moduleProperty(product, "minimumIosVersion");
    if (minimumiOSVersion && product.moduleProperty("qbs", "targetOS").contains("ios")) {
        if (product.moduleProperty("qbs", "targetOS").contains("ios-simulator"))
            args.push('-mios-simulator-version-min=' + minimumiOSVersion);
        else
            args.push('-miphoneos-version-min=' + minimumiOSVersion);
    }

    return args
}

function majorVersion(version, defaultValue)
{
    var n = parseInt(product.version, 10);
    return isNaN(n) ? defaultValue : n;
}

function soname(product, outputFilePath)
{
    var outputFileName = FileInfo.fileName(outputFilePath);
    if (product.version) {
        var major = majorVersion(product.version);
        if (major) {
            return outputFileName.substr(0, outputFileName.length - product.version.length)
                    + major;
        }
    }
    return outputFileName;
}

// Returns the GCC language name equivalent to fileTag, accepted by the -x argument
function languageName(fileTag)
{
    if (fileTag === 'c')
        return 'c';
    else if (fileTag === 'cpp')
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

function prepareCompiler(project, product, inputs, outputs, input, output)
{
    var i, c;

    // Determine which C-language we're compiling
    var tag = ModUtils.fileTagForTargetLanguage(input.fileTags.concat(output.fileTags));
    if (!["c", "cpp", "objc", "objcpp", "asm", "asm_cpp"].contains(tag))
        throw ("unsupported source language: " + tag);

    // Whether we're compiling a precompiled header or normal source file
    var pchOutput = outputs[tag + "_pch"] ? outputs[tag + "_pch"][0] : undefined;

    var args = configFlags(input);
    args.push('-pipe');

    var visibility = ModUtils.moduleProperty(product, 'visibility');
    if (!product.type.contains('staticlibrary')
            && !product.moduleProperty("qbs", "toolchain").contains("mingw")) {
        if (visibility === 'hidden')
            args.push('-fvisibility=hidden');
        if (visibility === 'hiddenInlines')
            args.push('-fvisibility-inlines-hidden');
        if (visibility === 'default')
            args.push('-fvisibility=default')
    }

    var prefixHeaders = ModUtils.moduleProperty(product, "prefixHeaders");
    for (i in prefixHeaders) {
        args.push('-include');
        args.push(prefixHeaders[i]);
    }

    args = args.concat(ModUtils.moduleProperties(input, 'platformCommonCompilerFlags'));

    args.push('-x');
    args.push(Gcc.languageName(tag) + (pchOutput ? '-header' : ''));
    args = args.concat(ModUtils.moduleProperties(input, 'platformFlags', tag),
                       ModUtils.moduleProperties(input, 'flags', tag));

    if (!pchOutput && ModUtils.moduleProperty(product, 'precompiledHeader', tag)) {
        var pchFilePath = FileInfo.joinPaths(
            ModUtils.moduleProperty(product, "precompiledHeaderDir"),
            product.name + "_" + tag);
        args.push('-include', pchFilePath);
    }

    args = args.concat(ModUtils.moduleProperties(input, 'commonCompilerFlags'));
    args = args.concat(additionalCompilerFlags(product, input, output));
    args = args.concat(additionalCompilerAndLinkerFlags(product));

    var compilerPath = ModUtils.moduleProperty(product, "compilerPath");
    var wrapperArgs = ModUtils.moduleProperty(product, "compilerWrapper");
    if (wrapperArgs && wrapperArgs.length > 0) {
        args.unshift(compilerPath);
        compilerPath = wrapperArgs.shift();
        args = wrapperArgs.concat(args);
    }

    var cmd = new Command(compilerPath, args);
    cmd.description = (pchOutput ? 'pre' : '') + 'compiling ' + FileInfo.fileName(input.fileName);
    if (pchOutput)
        cmd.description += ' (' + tag + ')';
    cmd.highlight = "compiler";
    cmd.responseFileUsagePrefix = '@';
    return cmd;
}
