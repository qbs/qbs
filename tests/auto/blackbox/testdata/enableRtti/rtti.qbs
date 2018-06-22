Project {
    property bool treatAsObjcpp: false
    CppApplication {
        cpp.cxxLanguageVersion: "c++11"
        cpp.treatWarningsAsErrors: true
        Group {
            files: ["main.cpp"]
            fileTags: [project.treatAsObjcpp ? "objcpp" : "cpp"]
        }
    }
}
