// helper functions for the Qt modules

function getLibraryName(qtModule, qtcore, qbs)
{
    var libName = "Qt";
    if (qtcore.versionMajor >= 5 && !qtcore.frameworkBuild)
        libName += qtcore.versionMajor;
    libName += qtModule;
    if (qbs.targetOS === 'windows') {
        libName += (qbs.enableDebugCode ? 'd' : '');
        if (qtcore.versionMajor < 5)
            libName += qtcore.versionMajor;
        if (qbs.toolchain !== "mingw")
            libName += '.lib';
    }
    return libName;
}
