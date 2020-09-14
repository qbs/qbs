import "../BareMetalApplication.qbs" as BareMetalApplication

BareMetalApplication {
    condition: {
        console.info("current toolset: %%"
            + qbs.toolchainType + "%%, %%" + qbs.architecture + "%%");
        return true;
    }
    cpp.generateLinkerMapFile: true
    files: ["main.c"]
}
