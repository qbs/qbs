import qbs 1.0

Project {
    QtApplication {
        condition: qbs.targetOS.contains("osx")
        files: "main.mm"
        cpp.frameworks: [ "Foundation" ]
    }
}
