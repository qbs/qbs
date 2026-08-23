Project {
    ObjectLibrary {
        name: "obj1"
        Depends { name: "cpp" }
        files: "obj1.cpp"
    }

    ObjectLibrary {
        name: "obj2"
        Depends { name: "cpp" }
        files: "obj2.cpp"
    }

    StaticLibrary {
        name: "static"
        Depends { name: "cpp" }
        Depends { name: "obj1" }
        Depends { name: "obj2" }
    }

    CppApplication {
        name: "app"
        consoleApplication: true
        Depends { name: "cpp" }
        Depends { name: "static" }
        files: "main.cpp"
    }
}
