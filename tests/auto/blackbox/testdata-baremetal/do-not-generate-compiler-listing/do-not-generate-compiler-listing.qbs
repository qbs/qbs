import "../BareMetalApplication.qbs" as BareMetalApplication

BareMetalApplication {
    condition: {
        if (!qbs.toolchain.contains("gcc")) {
            if (cpp.compilerName.startsWith("armcc"))
                console.info("using short listing file names");
            return true;
        }
        console.info("unsupported toolset: %%"
            + qbs.toolchainType + "%%, %%" + qbs.architecture + "%%");
        return false;
    }
    cpp.generateCompilerListingFiles: false
    files: ["main.c", "fun.c"]
}
