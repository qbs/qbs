Project {
    CppApplication {
        name: "p1"
        consoleApplication: true
        cpp.includePaths: ["."]
        files: [
            "p1.cpp",
            "shared.h",
        ]
    }

    CppApplication {
        name: "p2"
        consoleApplication: true
        cpp.includePaths: ["."]
        files: [
            "p2.cpp",
            "shared.h",
        ]
    }

    CppApplication {
        name: "p3"
        consoleApplication: true
        cpp.includePaths: [".", "subdir"]
        files: [
            "p3.cpp",
        ]
    }
}
