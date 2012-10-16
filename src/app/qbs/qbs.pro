QT = core script
greaterThan(QT_MAJOR_VERSION, 4): QT += concurrent
TEMPLATE = app
TARGET = qbs
DESTDIR = ../../../bin

CONFIG   += console
CONFIG   -= app_bundle

SOURCES += main.cpp \
    ctrlchandler.cpp \
    application.cpp \
    ../shared/commandlineparser.cpp
HEADERS += \
    ctrlchandler.h \
    application.h \
    status.h \
    ../shared/commandlineparser.h

include(../../lib/use.pri)
include(../../../qbs_version.pri)
