CppApplication {
    condition: false
    files: "main.cpp"
    Group {
        condition: qbs.targetOS.contains("stuff")
        qbs.install: false
    }
}
