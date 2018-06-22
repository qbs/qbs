QtApplication {
    name: "debuggable-app"
    consoleApplication: true
    Depends { name: "Qt.quick" }
    Qt.quick.qmlDebugging: true
    files: "main.cpp"
}
