CppApplication {
    name: "theapp"
    files: [
        "combinable.cpp",
        "main.cpp",
    ]
    Group {
        files: ["uncombinable.cpp"]
        fileTags: ["cpp"]
    }
}
