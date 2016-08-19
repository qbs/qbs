import qbs

Project {
    minimumQbsVersion: "1.6"
    CppApplication {
        cpp.dynamicLibraries: [undefined, ""]
        cpp.staticLibraries: [null, []]
        files: ["main.cpp"]
    }
}
