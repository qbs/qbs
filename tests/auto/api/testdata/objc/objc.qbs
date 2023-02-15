Project {
    CppApplication {
        condition: qbs.targetOS.includes("macos")
        files: "main.mm"
        cpp.frameworks: [ "Foundation" ]
    }
}
