import "../BareMetalApplication.qbs" as BareMetalApplication

BareMetalApplication {
    condition: {
        if (qbs.toolchainType === "sdcc") {
            console.info("unsupported toolset: %%"
                + qbs.toolchainType + "%%, %%" + qbs.architecture + "%%");
            return false;
        }
        console.info("current toolset: %%"
            + qbs.toolchainType + "%%, %%" + qbs.architecture + "%%");
        return true;
    }
    cpp.generateLinkerMapFile: true
    files: ["main.c"]
}
