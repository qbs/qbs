import "../BareMetalApplication.qbs" as BareMetalApplication

BareMetalApplication {
    cpp.defines: ["FOO", "BAR"]
    files: ["main.c"]
}
