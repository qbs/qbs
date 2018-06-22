CppApplication {
    Depends { name: "lex_yacc" }
    lex_yacc.outputTag: "cpp"
    lex_yacc.yaccFlags: ["-l"]
    cpp.includePaths: [".", ".."]
    cpp.cxxLanguageVersion: "c++11"
    cpp.minimumMacosVersion: "10.7"
    consoleApplication: true
    files: [
        "lexer.l",
        "parser.y",
        "types.h",
    ]
}
