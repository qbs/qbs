Project {
    property bool treatAsObjcpp: false
    CppApplication {
        consoleApplication: true
        cpp.cxxLanguageVersion: "c++11"
        cpp.treatWarningsAsErrors: true
        Group {
            files: ["main.cpp"]
            fileTags: [project.treatAsObjcpp ? "objcpp" : "cpp"]
        }
    }
}
