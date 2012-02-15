QT = core script gui

TEMPLATE = app
TARGET = qbs-graph
DESTDIR = ../../../bin/

CONFIG   += console
CONFIG   -= app_bundle

SOURCES += graph.cpp

include(../../lib/use.pri)
