Project {
    CppApplication {
        name: "a"
        consoleApplication: true
        targetName: "theName"
        destinationDirectory: project.buildDirectory
        files: ["main.cpp"]
    }
    CppApplication {
        name: "b"
        consoleApplication: true
        targetName: "theName"
        destinationDirectory: project.buildDirectory
        files: ["main.cpp"]
    }
}
