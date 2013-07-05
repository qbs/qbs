import qbs 1.0

PathProbe {
    pathSuffixes: [ "include" ]
    platformEnvironmentPaths: {
        if (qbs.toolchain.contains('msvc'))
            return [ "INCLUDE" ];
        return undefined;
    }
}
