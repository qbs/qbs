import "../BareMetalApplication.qbs" as BareMetalApplication

BareMetalApplication {
    condition: {
        if (qbs.toolchainType === "keil") {
            if (qbs.architecture === "mcs51")
                return true;
        }
        console.info("unsupported toolset: %%"
            + qbs.toolchainType + "%%, %%" + qbs.architecture + "%%");
        return false;
    }
    files: [qbs.architecture + "-" + qbs.toolchainType + ".asm"]
}
