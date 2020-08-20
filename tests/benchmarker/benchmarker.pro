TARGET = qbs_benchmarker
DESTDIR = ../../bin
CONFIG += console
CONFIG -= app_bundle
CONFIG += c++17
QT += concurrent
SOURCES = \
    benchmarker-main.cpp \
    benchmarker.cpp \
    commandlineparser.cpp \
    runsupport.cpp \
    valgrindrunner.cpp

HEADERS = \
    activities.h \
    benchmarker.h \
    commandlineparser.h \
    exception.h \
    runsupport.h \
    valgrindrunner.h
