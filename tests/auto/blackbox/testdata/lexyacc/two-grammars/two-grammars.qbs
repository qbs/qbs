import qbs.Host

CppApplication {
    Depends { name: "lex_yacc" }
    consoleApplication: true
    cpp.includePaths: ".."
    Properties {
        condition: Host.os().includes("darwin") && qbs.toolchain.includes("clang")
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
