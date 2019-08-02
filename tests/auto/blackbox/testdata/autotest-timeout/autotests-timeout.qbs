Project {
    CppApplication {
        name: "testApp"
        type: ["application", "autotest"]
        Depends { name: "autotest" }
        cpp.cxxLanguageVersion: "c++11"
        files: "test-main.cpp"
    }
    AutotestRunner {}
}
