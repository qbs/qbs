// helper functions for the Qt modules

function getLibraryName(qtModule, versionMajor, qbs, cpp)
{
    var libName = qtModule;
    if (qbs.targetOS === 'windows') {
        libName = qtModule + (cpp.debugInformation ? 'd' : '') + versionMajor;
        if (qbs.toolchain !== "mingw")
            libName += '.lib';
    }
    return libName;
}
