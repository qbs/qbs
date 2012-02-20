// helper functions for the Qt modules

function getLibraryName(qtModule, targetOS, debugInfo)
{
    var libName
    if (targetOS === 'linux' || targetOS === 'mac') {
        libName = qtModule
    } else if (targetOS === 'windows') {
        libName = qtModule + (debugInfo ? 'd' : '') + '4'
        if (qbs.toolchain !== "mingw")
            libName += '.lib'
    }
    return libName
}
