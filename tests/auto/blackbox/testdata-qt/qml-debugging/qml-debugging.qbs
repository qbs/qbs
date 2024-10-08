QtApplication {
    name: "debuggable-app"
    consoleApplication: true
    Depends { name: "Qt.quick" }
    Qt.quick.qmlDebugging: true
    files: "main.cpp"
    Probe {
        id: checker
        property bool isGcc: qbs.toolchain.contains("gcc") && !qbs.toolchain.contains("emscripten")
        property string executableSuffix: cpp.executableSuffix
        configure: {
            console.info("is gcc: " + isGcc);
            console.info("executable suffix: " + executableSuffix);
        }
    }
}
