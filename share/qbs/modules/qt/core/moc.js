function args(product, input, outputFileName)
{
    var defines = ModUtils.modulePropertiesFromArtifacts(product, [input], 'cpp', 'compilerDefines');
    defines = defines.concat(ModUtils.modulePropertiesFromArtifacts(product, [input], 'cpp', 'platformDefines'));
    defines = defines.concat(ModUtils.modulePropertiesFromArtifacts(product, [input], 'cpp', 'defines'));
    var includePaths = ModUtils.modulePropertiesFromArtifacts(product, [input], 'cpp', 'includePaths');
    var args = [];
    args = args.concat(
                defines.map(function(item) { return '-D' + item; }),
                includePaths.map(function(item) { return '-I' + item; }),
                '-o', outputFileName,
                input.fileName);
    return args;
}

function fullPath(product)
{
    return ModUtils.moduleProperty(product, "binPath") + '/' + ModUtils.moduleProperty(product, "mocName");
}
