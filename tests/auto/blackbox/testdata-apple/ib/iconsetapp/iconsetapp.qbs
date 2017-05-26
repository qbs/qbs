import qbs

CppApplication {
    Depends { name: "ib" }
    files: ["main.c", "white.iconset"]
}
