import qbs.Probes

Product {
    id: product
    condition: qbs.toolchainType === "sdcc"

    Depends { name: "cpp" }

    Probes.SdccProbe {
        id: probe
        compilerFilePath: cpp.compilerPath
        enableDefinesByLanguage: cpp.enableCompilerDefinesByLanguage
        preferredArchitecture: qbs.architecture
    }

    property bool dummy: {
        if (!product.condition)
            return;
        if (!probe.found
                || !probe.endianness
                || !probe.compilerDefinesByLanguage
                || !probe.includePaths
                || (probe.includePaths.length === 0)
                || (qbs.architecture !== probe.architecture)) {
            console.info("broken probe: %%" + qbs.toolchainType + "%%, %%"
                         + qbs.architecture + "%%");
        }
    }
}
