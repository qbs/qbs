import "../BareMetalApplication.qbs" as BareMetalApplication

BareMetalApplication {
    files: ["main.c"]
    property bool dummy: {
        console.info("compilerIncludePaths: %%" + cpp.compilerIncludePaths + "%%");
        return true;
    }
}
