import qbs

Project {
    CppApplication {
        name: "default app"
        type: ["application"]
        files: "main.cpp"
    }

    CppApplication {
        name: "non-default app"
        type: ["application"]
        builtByDefault: false
        files: "main.cpp"
    }
}
