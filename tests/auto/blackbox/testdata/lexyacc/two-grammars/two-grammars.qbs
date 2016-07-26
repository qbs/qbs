import qbs

CppApplication {
    Depends { name: "lex_yacc" }
    consoleApplication: true
    files: [
        "g1.l",
        "g1.y",
        "g2.l",
        "g2.y",
        "main.c",
    ]
}
