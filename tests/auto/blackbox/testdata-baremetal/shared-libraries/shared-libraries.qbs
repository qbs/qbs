import "../BareMetalApplication.qbs" as BareMetalApplication

Project {
    condition: {
        if (qbs.targetPlatform === "windows" && qbs.architecture === "x86") {
            if (qbs.toolchainType === "watcom")
                return true;
            if (qbs.toolchainType === "dmc")
                return true;
        }

        if (qbs.toolchainType === "msvc")
            return true;

        console.info("unsupported toolset: %%"
            + qbs.toolchainType + "%%, %%" + qbs.architecture + "%%");
        return false;
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        destinationDirectory: "bin"
        name: "shared"
        files: ["shared.c"]
    }
    BareMetalApplication {
        Depends { name: "shared" }
        destinationDirectory: "bin"
        name: "app"
        files: ["app.c"]
    }
}
