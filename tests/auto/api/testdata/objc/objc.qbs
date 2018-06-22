Project {
    CppApplication {
        condition: qbs.targetOS.contains("macos")
        files: "main.mm"
        cpp.frameworks: [ "Foundation" ]
    }
}
