QtApplication {
    name: "debuggable-app"
    consoleApplication: true
    Depends { name: "Qt.quick" }
    Qt.quick.qmlDebugging: true
    files: "main.cpp"
    Probe {
        id: checker
        property bool isGcc: qbs.toolchain.contains("gcc")
        configure: { console.info("is gcc: " + isGcc); }
    }
}
