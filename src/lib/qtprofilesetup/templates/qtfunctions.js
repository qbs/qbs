// helper functions for the Qt modules

function getPlatformLibraryName(name, qtcore, qbs)
{
    var libName = name;
    if (qbs.targetOS.contains('windows')) {
        libName += (qbs.enableDebugCode ? 'd' : '');
        if (qtcore.versionMajor < 5)
            libName += qtcore.versionMajor;
        if (!qbs.toolchain.contains("mingw"))
            libName += '.lib';
    }
    if (qbs.targetOS.contains("darwin")) {
        if (!qtcore.frameworkBuild && qtcore.buildVariant.contains("debug") &&
                (!qtcore.buildVariant.contains("release") || qbs.enableDebugCode))
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
