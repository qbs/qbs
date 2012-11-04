// helper functions for the Qt modules

function getLibraryName(qtModule, versionMajor, targetOS, debugInfo)
{
    var libName = qtModule;
    if (targetOS === 'windows') {
        libName = qtModule + (debugInfo ? 'd' : '') + versionMajor;
        if (qbs.toolchain !== "mingw")
            libName += '.lib';
    }
    return libName;
}
