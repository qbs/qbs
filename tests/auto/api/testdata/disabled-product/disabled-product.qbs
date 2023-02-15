CppApplication {
    condition: false
    files: "main.cpp"
    Group {
        condition: qbs.targetOS.includes("stuff")
        qbs.install: false
    }
}
