CppApplication {
    name: "foo"
    version: "1.0.0"
    Depends { name: "Qt.core" }
    Depends { name: "Qt.network" }
    consoleApplication: true
    install: true
    files: "foo.cpp"
}
