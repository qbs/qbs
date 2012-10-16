QT = core script gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets concurrent

TEMPLATE = app
TARGET = qbs-graph
DESTDIR = ../../../bin/

CONFIG   += console
CONFIG   -= app_bundle

HEADERS += ../shared/commandlineparser.h
SOURCES += graph.cpp ../shared/commandlineparser.cpp

include(../../lib/use.pri)
include(../../../qbs_version.pri)
