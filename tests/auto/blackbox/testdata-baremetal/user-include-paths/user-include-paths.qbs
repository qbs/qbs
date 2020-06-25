import "../BareMetalApplication.qbs" as BareMetalApplication

BareMetalApplication {
    files: ["main.c"]
    cpp.includePaths: ["foo", "bar"]
}
