import qbs 1.0

Project {
    CppApplication {
        condition: qbs.targetOS === "osx"
        Depends { name: "Qt.core" }
        files: "main.mm"
        cpp.frameworks: [ "Foundation" ]
    }
}
