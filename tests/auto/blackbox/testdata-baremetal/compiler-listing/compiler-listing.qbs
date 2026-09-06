import "../BareMetalApplication.qbs" as BareMetalApplication

BareMetalApplication {
    condition: {
        if (qbs.toolchain.includes("gcc") || qbs.toolchainType === "watcom") {
            console.info("unsupported toolset: %%"
                + qbs.toolchainType + "%%, %%" + qbs.architecture + "%%");
            return false;
        }
        console.info("compiler listing suffix: %%" + cpp.compilerListingSuffix + "%%");
        return true;
    }
    files: ["main.c", "fun.c"]
}
