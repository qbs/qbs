// helper functions for the Qt modules

function getLibraryName(qtModule, versionMajor, qbs, cpp)
{
    var libName = "Qt";
    if (versionMajor >= 5)
        libName += versionMajor;
    libName += qtModule;
    if (qbs.targetOS === 'windows') {
        libName += (cpp.debugInformation ? 'd' : '');
        if (versionMajor < 5)
            libName += versionMajor;
        if (qbs.toolchain !== "mingw")
            libName += '.lib';
    }
    return libName;
}
