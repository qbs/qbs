include(../../plugins.pri)
DEFINES += CPLUSPLUS_NO_PARSER

TARGET = qbs_cpp_scanner

QT = core

HEADERS += CPlusPlusForwardDeclarations.h Lexer.h Token.h ../scanner.h \
           cpp_global.h
SOURCES += Lexer.cpp Token.cpp \
    cppscanner.cpp
