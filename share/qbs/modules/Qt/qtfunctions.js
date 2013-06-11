// helper functions for the Qt modules

function getPlatformLibraryName(name, qtcore, qbs)
{
    var libName = name;
    if (qbs.targetOS === 'windows') {
        libName += (qbs.enableDebugCode ? 'd' : '');
        if (qtcore.versionMajor < 5)
            libName += qtcore.versionMajor;
        if (qbs.toolchain !== "mingw")
            libName += '.lib';
    }
    if (qbs.targetPlatform.indexOf("darwin") !== -1) {
        if (qtcore.buildVariant.indexOf("debug") !== -1 &&
                (qtcore.buildVariant.indexOf("release") === -1 || qbs.enableDebugCode))
            libName += '_debug';
    }
    return libName;
}

function getQtLibraryName(qtModule, qtcore, qbs)
{
    var libName = "Qt";
    if (qtcore.versionMajor >= 5 && !qtcore.frameworkBuild)
        libName += qtcore.versionMajor;
    libName += qtModule;
    return getPlatformLibraryName(libName, qtcore, qbs);
}
