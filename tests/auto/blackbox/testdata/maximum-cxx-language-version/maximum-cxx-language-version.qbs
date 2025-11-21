CppApplication {
    name: "app"
    consoleApplication: true
    property bool enableNewestModule: true

    Depends { name: "oldmodule" }
    Depends { name: "newermodule" }
    Depends { name: "newestmodule"; condition: enableNewestModule }

    files: "main.cpp"
}
