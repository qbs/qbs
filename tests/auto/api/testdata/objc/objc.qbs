import qbs 1.0

Project {
    CppApplication {
        condition: qbs.targetOS.contains("macos")
        files: "main.mm"
        cpp.frameworks: [ "Foundation" ]
    }
}
