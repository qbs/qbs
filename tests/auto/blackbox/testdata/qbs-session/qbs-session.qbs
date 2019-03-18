Project {
    StaticLibrary {
        name: "theLib"
        Depends { name: "cpp" }
        cpp.cxxLanguageVersion: "c++11"
        Group {
            name: "sources"
            files: "lib.cpp"
        }
        Group {
            name: "headers"
            files: "lib.h"
        }
    }
    CppApplication {
        name: "theApp"
        consoleApplication: true
        Depends { name: "mymodule" }
        cpp.cxxLanguageVersion: "c++14"
        cpp.warningLevel: "all"
        files: "main.cpp"
        install: true
    }
}

