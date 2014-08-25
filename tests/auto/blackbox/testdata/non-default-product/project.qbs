import qbs

Project {
    CppApplication {
        name: "default app"
        files: "main.cpp"
    }

    CppApplication {
        name: "non-default app"
        builtByDefault: false
        files: "main.cpp"
    }
}
