function collectDynamicLinkPaths(config) {
    var paths = []

    var linkProducts = config.linkProducts || []
    for (var libPath in linkProducts) {
        var linkedConfig = linkProducts[libPath]
        var fileTags = linkedConfig['fileTags']
        if (fileTags.indexOf('dynamiclibrary') >= 0) {
            paths.push(FileInfo.path(libPath))
            paths = paths.concat(collectDynamicLinkPaths(linkedConfig))
        }
    }
    return paths
}

function linkEnv(cmd) {
    // ### setup LD_LIBRARY_PATH for ProductModule dynamiclibrary depends
//    var paths = []
//    for (var i in config.linkProducts) {
//        var linkedConfig = config.linkProducts[i]
//        var fileTags = linkedConfig['fileTags']
//        if (fileTags.indexOf('dynamiclibrary') >= 0) {
//            paths = paths.concat(collectDynamicLinkPaths(linkedConfig))
//        }
//    }

//    if (paths.length) {
//        var envdef = newEnvironmentDefinition("LD_LIBRARY_PATH")
//        envdef.pathNames = true
//        envdef.separator = ':'
//        envdef.prepend = paths
//        cmd.addEnvironmentDefinition(envdef)
//    }
}

function libs(libraryPaths, rpaths, dynamicLibraries, staticLibraries) {

    var args = [];
    if (rpaths && rpaths.length)
        args.push('-Wl,-rpath,' + rpaths.join(':'));
    for (var i in libraryPaths) {
        args.push('-L' + libraryPaths[i]);
    }
    for (var i in staticLibraries) {
        args.push(staticLibraries[i]);
    }
    for (var i in dynamicLibraries) {
        if (FileInfo.isAbsolutePath(dynamicLibraries[i])) {
            args.push(dynamicLibraries[i]);
        } else {
            args.push('-l' + dynamicLibraries[i]);
        }
    }
    return args;
}

function macLibs(libraryPaths, rpaths, dynamicLibraries, staticLibraries, frameworks)
{
    var args = libs(libraryPaths, rpaths, dynamicLibraries, staticLibraries);
    for (var i in libraryPaths) {
        args.push('-F' + libraryPaths[i]);
    }
    for (var i in frameworks) {
        args.push('-framework');
        args.push(frameworks[i]);
    }
    return args;
}

function configFlags(config) {
    var args = [];
    // optimization:
    if (config.module.debugInformation === 'on')
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
        args.push('-W');
    }
    if (config.module.treatWarningsAsErrors)
        args.push('-Werror');

    return args;
}

function frameworkPaths(config) {
    var args = [];
    for (var i in config.frameworkPaths) {
        args.push('-F' + config.frameworkPaths[i]);
    }
    return args;
}

function frameworks(config) {
    var args = [];
    for (var i in config.frameworks) {
        args.push('-framework');
        args.push(config.frameworks[i]
                .replace(/.framework$/,''));
    }
    return args;
}

function describe(cmd, input, output)
{
    cmd.description += FileInfo.fileName(input);
}
