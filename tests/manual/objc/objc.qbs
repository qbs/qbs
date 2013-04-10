import qbs 1.0

Project {
    CppApplication {
        condition: qbs.targetOS === "mac"
        Depends { name: "Qt.core" }
        files: "main.mm"
        cpp.frameworks: [ "Foundation" ]
    }
}
