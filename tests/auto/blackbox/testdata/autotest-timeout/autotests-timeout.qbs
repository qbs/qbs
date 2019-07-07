Project {
    CppApplication {
        name: "testApp"
        type: ["application", "autotest"]
        Depends { name: "autotest" }
        files: "test-main.cpp"
    }
    AutotestRunner {}
}
