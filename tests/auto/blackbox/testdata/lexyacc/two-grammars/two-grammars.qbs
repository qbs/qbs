CppApplication {
    Depends { name: "lex_yacc" }
    consoleApplication: true
    cpp.includePaths: ".."
    Properties {
        condition: qbs.hostOS.contains("darwin") && qbs.toolchain.contains("clang")
        cpp.cFlags: "-Wno-implicit-function-declaration"
    }
    files: [
        "g1.l",
        "g1.y",
        "g2.l",
        "g2.y",
        "main.c",
    ]
}
