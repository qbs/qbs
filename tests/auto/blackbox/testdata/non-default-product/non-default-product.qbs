Project {
    CppApplication {
        name: "default app"
        consoleApplication: true
        property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }
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
