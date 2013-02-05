DEFINES += CPLUSPLUS_NO_PARSER

DESTDIR= ../../../../plugins/
TEMPLATE = lib
TARGET = qbs_cpp_scanner

QT = core

CONFIG += depend_includepath
unix: CONFIG += plugin

HEADERS += CPlusPlusForwardDeclarations.h Lexer.h Token.h ../scanner.h \
           cpp_global.h
SOURCES += Lexer.cpp Token.cpp \
    cppscanner.cpp
