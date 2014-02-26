TARGET = qbs_fuzzy-test
DESTDIR = ../../bin
CONFIG += console
CONFIG -= app_bundle
SOURCES = main.cpp \
    commandlineparser.cpp \
    fuzzytester.cpp

HEADERS += \
    commandlineparser.h \
    fuzzytester.h
