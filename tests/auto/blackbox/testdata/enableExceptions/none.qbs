CppApplication {
    files: ["emptymain.cpp"]

    Group {
        condition: qbs.targetOS.contains("darwin")
        files: ["empty.m", "empty.mm"]
    }
}
