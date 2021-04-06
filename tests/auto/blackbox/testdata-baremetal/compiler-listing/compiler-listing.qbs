import "../BareMetalApplication.qbs" as BareMetalApplication

BareMetalApplication {
    condition: {
        if (!qbs.toolchain.contains("gcc")) {
            if (cpp.compilerName.startsWith("armcc"))
                console.info("using short listing file names");
            console.info("compiler listing suffix: %%" + cpp.compilerListingSuffix + "%%");
            return true;
        }
        console.info("unsupported toolset: %%"
            + qbs.toolchainType + "%%, %%" + qbs.architecture + "%%");
        return false;
    }
    files: ["main.c", "fun.c"]
}
