import qbs.base 1.0

PathProbe {
    pathSuffixes: [ "include" ]
    platformEnvironmentPaths: {
        if (qbs.toolchain === 'msvc')
            return [ "INCLUDE" ];
        return undefined;
    }
}
