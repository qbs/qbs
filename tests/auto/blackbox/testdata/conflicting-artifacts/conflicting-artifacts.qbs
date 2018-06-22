Project {
    CppApplication {
        name: "a"
        targetName: "theName"
        destinationDirectory: project.buildDirectory
        files: ["main.cpp"]
    }
    CppApplication {
        name: "b"
        targetName: "theName"
        destinationDirectory: project.buildDirectory
        files: ["main.cpp"]
    }
}
