Project {
StaticLibrary {
    name: "a"
    Depends { name: "cpp" }
    files: ["lib.cpp"]
    }

Product {
    type: qbs.targetOS.includes("darwin") ? undefined : ["staticlibrary"]
    name: "b"
    Depends { name: "cpp" }
    Depends { name: "a" }
}
}
