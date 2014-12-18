import qbs

Project {
    CppApplication {
        name: "default app"
        consoleApplication: true
        files: "main.cpp"
    }

    CppApplication {
        name: "non-default app"
        consoleApplication: true
        builtByDefault: false
        files: "main.cpp"
    }
}
