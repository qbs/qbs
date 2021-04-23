import "../BareMetalApplication.qbs" as BareMetalApplication

BareMetalApplication {
    property bool dummy: {
        console.info("linker map suffix: %%" + cpp.linkerMapSuffix + "%%");
    }
    files: ["main.c"]
}
