DEFINES += CPLUSPLUS_NO_PARSER

DESTDIR= ../../../../plugins/
TEMPLATE = lib
TARGET = qbs_cpp_scanner
DEPENDPATH += .
INCLUDEPATH += .

QT = core

unix: CONFIG += plugin

HEADERS += CPlusPlusForwardDeclarations.h Lexer.h Token.h ../scanner.h \
           cpp_global.h
SOURCES += cpp.cpp Lexer.cpp Token.cpp
