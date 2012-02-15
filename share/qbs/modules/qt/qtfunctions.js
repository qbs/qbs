// helper functions for the Qt modules

function getLibraryName(qtModule, targetOS, debugInfo)
{
    var isUnix = (targetOS == 'linux' || targetOS == 'mac')
    var libName
    if (isUnix)
        libName = qtModule
    else if (targetOS == 'windows')
        libName = qtModule + (debugInfo ? 'd' : '') + '4.lib'
    return libName
}
