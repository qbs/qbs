import "../BareMetalApplication.qbs" as BareMetalApplication

BareMetalApplication {
    condition: {
        if (!qbs.toolchain.contains("gcc")) {
            console.info("compiler listing suffix: %%" + cpp.compilerListingSuffix + "%%");
            return true;
        }
        console.info("unsupported toolset: %%"
            + qbs.toolchainType + "%%, %%" + qbs.architecture + "%%");
        return false;
    }
    files: ["main.c", "fun.c"]
}
