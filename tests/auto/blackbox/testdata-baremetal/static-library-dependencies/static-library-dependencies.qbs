Project {
    StaticLibrary {
        name: "lib-a"
        Depends { name: "cpp" }
        files: ["a1.c", "a2.c"]
    }
    StaticLibrary {
        name: "lib-b"
        Depends { name: "cpp" }
        Depends { name: "lib-a" }
        files: ["b.c"]
    }
    StaticLibrary {
        name: "lib-c"
        Depends { name: "cpp" }
        Depends { name: "lib-a" }
        files: ["c.c"]
    }
    StaticLibrary {
        name: "lib-d"
        Depends { name: "cpp" }
        Depends { name: "lib-b" }
        Depends { name: "lib-c" }
        files: ["d.c"]
    }
    StaticLibrary {
        name: "lib-e"
        Depends { name: "cpp" }
        Depends { name: "lib-d" }
        files: ["e.c"]
    }
    CppApplication {
        name: "app"
        Depends { name: "lib-e" }
        files: ["app.c"]
    }
}
