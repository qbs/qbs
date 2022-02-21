import qbs.Probes

Product {
    id: product
    condition: qbs.toolchainType === "dmc"

    Depends { name: "cpp" }

    Probes.DmcProbe {
        id: probe
        compilerFilePath: cpp.compilerPath
        enableDefinesByLanguage: cpp.enableCompilerDefinesByLanguage
        _targetPlatform: qbs.targetPlatform
        _targetArchitecture: qbs.architecture
        _targetExtender: cpp.extenderName
    }

    property bool dummy: {
        if (!product.condition)
            return;
        if (!probe.found
                || !probe.compilerDefinesByLanguage
                || !probe.includePaths
                || (probe.includePaths.length === 0)
                || (qbs.architecture !== probe.architecture)
                || (qbs.targetPlatform !== probe.targetPlatform)) {
            console.info("broken probe: %%" + qbs.toolchainType + "%%, %%"
                         + qbs.architecture + "%%");
        }
    }
}
