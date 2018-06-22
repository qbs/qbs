Project {
    CppApplication {
        name: "default app"
        consoleApplication: true
        files: "main.cpp"
    }

    CppApplication {
        name: "non-default app"
        Depends { name: "default app" }
        consoleApplication: true
        builtByDefault: false
        files: "main.cpp"
    }
}
