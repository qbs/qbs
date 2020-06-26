import "../BareMetalApplication.qbs" as BareMetalApplication

BareMetalApplication {
    cpp.prefixHeaders: ["preinclude.h"]
    files: ["main.c"]
}
