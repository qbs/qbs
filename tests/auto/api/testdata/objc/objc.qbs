import qbs 1.0

Project {
    QtApplication {
        condition: qbs.targetOS.contains("macos")
        files: "main.mm"
        cpp.frameworks: [ "Foundation" ]
    }
}
