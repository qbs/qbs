import qbs

CppApplication {
    name: "debuggable-app"
    type: "application"
    Depends { name: "Qt.quick" }
    Qt.quick.qmlDebugging: true
    files: "main.cpp"
}
