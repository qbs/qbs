CppApplication {
    consoleApplication: true
    files: ["emptymain.cpp"]

    Group {
        condition: qbs.targetOS.includes("darwin")
        files: ["empty.m", "empty.mm"]
    }
}
