import qbs

CppApplication {
    Depends { name: "vcs" }
    files: ["main.cpp"]
}
