import "../BareMetalApplication.qbs" as BareMetalApplication

BareMetalApplication {
    condition: {
        if (qbs.toolchainType === "iar")
            return true;
        if (qbs.toolchainType === "keil") {
            if (qbs.architecture === "mcs51"
                || qbs.architecture === "mcs251"
                || qbs.architecture === "c166") {
                return true;
            }
        }
        console.info("unsupported toolset: %%"
            + qbs.toolchainType + "%%, %%" + qbs.architecture + "%%");
        return false;
    }
    cpp.generateCompilerListingFiles: false
    files: ["main.c", "fun.c"]
}
