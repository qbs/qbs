CppApplication {
    name: "theapp"
    consoleApplication: true
    files: [
        "combinable.cpp",
        "main.cpp",
    ]
    Group {
        files: ["uncombinable.cpp"]
        fileTags: ["cpp"]
    }
}
