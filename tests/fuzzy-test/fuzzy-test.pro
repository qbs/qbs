TARGET = qbs-fuzzy-test
DESTDIR = ../../bin
CONFIG += console
SOURCES = main.cpp \
    commandlineparser.cpp \
    fuzzytester.cpp

HEADERS += \
    commandlineparser.h \
    fuzzytester.h
