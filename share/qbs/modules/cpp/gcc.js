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
        args.push(staticLibraries[i]);
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
        args.push('-W');
    }
    if (config.module.treatWarningsAsErrors)
        args.push('-Werror');

    return args;
}

function describe(cmd, input, output)
{
    cmd.description += FileInfo.fileName(input);
}
