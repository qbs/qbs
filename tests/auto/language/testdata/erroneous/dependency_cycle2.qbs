Project {
    CppApplication {
        name: "A"
        Depends { name: "B" }
        files: ["main.cpp"]
    }
    CppApplication {
        name: "B"
        Depends { name: "C" }
        files: ["main.cpp"]
    }
    CppApplication {
        name: "C"
        Depends { name: "A" }
        files: ["main.cpp"]
    }
    CppApplication {
        name: "D"
        files: ["main.cpp"]
    }
}
