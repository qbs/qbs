import "../BareMetalApplication.qbs" as BareMetalApplication

BareMetalApplication {
    condition: {
        if (qbs.toolchainType === "keil") {
            if (qbs.architecture.startsWith("arm"))
                return true;
            if (qbs.architecture === "mcs51")
                return true;
        } else if (qbs.toolchainType === "iar") {
            if (qbs.architecture === "mcs51")
                return true;
        } else if (qbs.toolchainType === "sdcc") {
            if (qbs.architecture === "mcs51")
                return true;
        } else if (qbs.toolchainType === "gcc") {
            if (qbs.architecture.startsWith("arm"))
                return true;
        }
        console.info("unsupported toolset: %%"
            + qbs.toolchainType + "%%, %%" + qbs.architecture + "%%");
        return false;
    }
    files: [(qbs.architecture.startsWith("arm") ? "arm" : qbs.architecture)
                + "-" + qbs.toolchainType + ".s"]
}
