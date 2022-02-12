import qbs.Probes

Product {
    id: product
    condition: qbs.toolchainType === "watcom"

    Depends { name: "cpp" }

    Probes.WatcomProbe {
        id: probe
        compilerFilePath: cpp.compilerPath
        enableDefinesByLanguage: cpp.enableCompilerDefinesByLanguage
        _pathListSeparator: qbs.pathListSeparator
        _toolchainInstallPath: cpp.toolchainInstallPath
        _targetPlatform: qbs.targetPlatform
        _targetArchitecture: qbs.architecture
    }

    property bool dummy: {
        if (!product.condition)
            return;
        if (!probe.found
                || !probe.endianness
                || !probe.compilerDefinesByLanguage
                || !probe.environment
                || !probe.includePaths
                || (probe.includePaths.length === 0)
                || (qbs.architecture !== probe.architecture)
                || (qbs.targetPlatform !== probe.targetPlatform)) {
            console.info("broken probe: %%" + qbs.toolchainType + "%%, %%"
                         + qbs.architecture + "%%");
        }
    }
}
