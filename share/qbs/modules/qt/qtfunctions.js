// helper functions for the Qt modules

function getLibraryName(qtModule, versionMajor, targetOS, debugInfo)
{
    var libName
    if (targetOS === 'linux' || targetOS === 'mac') {
        libName = qtModule
    } else if (targetOS === 'windows') {
        libName = qtModule + (debugInfo ? 'd' : '') + versionMajor
        if (qbs.toolchain !== "mingw")
            libName += '.lib'
    }
    return libName
}
