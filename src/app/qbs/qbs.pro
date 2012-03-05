QT = core script
greaterThan(QT_MAJOR_VERSION, 4): QT += concurrent
TEMPLATE = app
TARGET = qbs
DESTDIR = ../../../bin

CONFIG   += console
CONFIG   -= app_bundle

SOURCES += main.cpp \
    ctrlchandler.cpp \
    application.cpp
HEADERS += \
    ctrlchandler.h \
    application.h \
    status.h

include(../../lib/use.pri)
