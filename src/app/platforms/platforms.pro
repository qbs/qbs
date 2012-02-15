QT = core script gui

TEMPLATE = app
TARGET = qbs-platforms
DESTDIR = ../../../bin/

CONFIG   += console
CONFIG   -= app_bundle

SOURCES += main.cpp probe.cpp

include(../../lib/use.pri)
