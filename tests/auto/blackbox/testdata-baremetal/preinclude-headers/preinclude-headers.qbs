import "../BareMetalApplication.qbs" as BareMetalApplication

BareMetalApplication {
    condition: {
        if (qbs.toolchainType === "keil") {
            if (qbs.architecture === "mcs51") {
                console.info("unsupported toolset: %%"
                    + qbs.toolchainType + "%%, %%" + qbs.architecture + "%%");
                return false;
            }
        }
        return true;
    }
    cpp.prefixHeaders: ["preinclude.h"]
    files: ["main.c"]
}
