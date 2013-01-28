QT       += core
QT       -= gui

TARGET = qbs-qmltypes
CONFIG   += console
CONFIG   -= app_bundle
DESTDIR = ../../../bin

TEMPLATE = app

SOURCES += \
    main.cpp \

HEADERS += ../shared/qbssettings.h

include(../../lib/use.pri)
