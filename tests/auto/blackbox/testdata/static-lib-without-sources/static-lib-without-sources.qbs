import qbs

Project {
StaticLibrary {
    name: "a"
    Depends { name: "cpp" }
    files: ["lib.cpp"]
    }

StaticLibrary {
    name: "b"
    Depends { name: "cpp" }
    Depends { name: "a" }
}
}
