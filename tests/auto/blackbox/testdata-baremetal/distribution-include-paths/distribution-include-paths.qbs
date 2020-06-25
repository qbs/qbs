import "../BareMetalApplication.qbs" as BareMetalApplication

BareMetalApplication {
    files: ["main.c"]
    cpp.distributionIncludePaths: ["foo", "bar"]
}
