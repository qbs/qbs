QT = core script gui

TEMPLATE = app
TARGET = qbs-platforms
DESTDIR = ../../../bin/

CONFIG   += console
CONFIG   -= app_bundle

HEADERS = msvcprobe.h
SOURCES += main.cpp probe.cpp msvcprobe.cpp

include(../../lib/use.pri)
