import qbs

Project {
    CppApplication {
        name: "p1"
        cpp.includePaths: ["."]
        files: [
            "p1.cpp",
            "shared.h",
        ]
    }

    CppApplication {
        name: "p2"
        cpp.includePaths: ["."]
        files: [
            "p2.cpp",
            "shared.h",
        ]
    }

    CppApplication {
        name: "p3"
        cpp.includePaths: [".", "subdir"]
        files: [
            "p3.cpp",
        ]
    }
}
