CppApplication {
    Depends { name: "lex_yacc" }
    consoleApplication: true
    cpp.includePaths: ".."
    files: [
        "g1.l",
        "g1.y",
        "g2.l",
        "g2.y",
        "main.c",
    ]
}
