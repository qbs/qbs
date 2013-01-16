QT = core script gui

TEMPLATE = app
TARGET = qbs-detect-toolchains
DESTDIR = ../../../bin/

CONFIG   += console
CONFIG   -= app_bundle

HEADERS = probe.h  msvcprobe.h
SOURCES += main.cpp probe.cpp msvcprobe.cpp

include(../../lib/use.pri)
