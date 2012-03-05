QT = core script gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets concurrent

TEMPLATE = app
TARGET = qbs-graph
DESTDIR = ../../../bin/

CONFIG   += console
CONFIG   -= app_bundle

SOURCES += graph.cpp

include(../../lib/use.pri)
