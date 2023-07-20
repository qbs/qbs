CppApplication {
    Depends { name: "mylib" }
    name: "My Application"
    targetName: "myapp"
    files: "main.c"
    version: "1.0.0"

    consoleApplication: true
    install: true
    installDebugInformation: true
}
