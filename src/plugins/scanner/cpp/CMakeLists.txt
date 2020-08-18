set(SOURCES
    ../scanner.h
    CPlusPlusForwardDeclarations.h
    Lexer.cpp
    Lexer.h
    Token.cpp
    Token.h
    cpp_global.h
    cppscanner.cpp
    )

add_qbs_plugin(qbs_cpp_scanner
    DEFINES "CPLUSPLUS_NO_PARSER"
    DEPENDS qbscore
    SOURCES ${SOURCES}
    )
