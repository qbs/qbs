include(cpp.pri)
include(../../plugins.pri)
DEFINES += CPLUSPLUS_NO_PARSER

QT = core

HEADERS += CPlusPlusForwardDeclarations.h Lexer.h Token.h ../scanner.h \
           cpp_global.h
SOURCES += Lexer.cpp Token.cpp \
    cppscanner.cpp
