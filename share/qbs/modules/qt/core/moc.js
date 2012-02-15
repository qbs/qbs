function args(input, output, config) {
    var args = [];

    // ### fixme
    var defines = ModUtils.appendAll_internal(config.modules, 'cpp', 'defines') //config.modules.cpp.compiler.defines
    var includePaths = ModUtils.appendAll_internal(config.modules, 'cpp', 'includePaths') //config.modules.cpp.compiler.includePaths
    for (var i in defines)
        args.push('-D' + defines[i]);
    for (var i in includePaths)
        args.push('-I' + includePaths[i]);
    args.push('-o', output);
    args.push(input);
    return args;
}

