import qbs

CppApplication {
    Depends { name: "lex_yacc" }
    lex_yacc.outputTag: "cpp"
    lex_yacc.yaccFlags: ["-l"]
    cpp.includePaths: ["."]
    cpp.cxxLanguageVersion: "c++11"
    files: [
        "lexer.l",
        "parser.y",
        "types.h",
    ]
}
