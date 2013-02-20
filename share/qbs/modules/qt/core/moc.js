function args(product, input, outputFileName)
{
    var defines = ModUtils.appendAllFromArtifacts(product, [input], 'cpp', 'compilerDefines');
    defines = defines.concat(ModUtils.appendAllFromArtifacts(product, [input], 'cpp', 'platformDefines'));
    defines = defines.concat(ModUtils.appendAllFromArtifacts(product, [input], 'cpp', 'defines'));
    var includePaths = ModUtils.appendAllFromArtifacts(product, [input], 'cpp', 'includePaths');
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
    return ModUtils.findFirst(product, "binPath") + '/' + ModUtils.findFirst(product, "mocName");
}
