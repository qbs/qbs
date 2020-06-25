import "../BareMetalApplication.qbs" as BareMetalApplication

BareMetalApplication {
    files: ["main.c"]
    cpp.systemIncludePaths: ["foo", "bar"]
}
