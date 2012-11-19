QT       += core
QT       -= gui

TARGET = qbs-qmltypes
CONFIG   += console
CONFIG   -= app_bundle
DESTDIR = ../../../bin

TEMPLATE = app

SOURCES += \
    main.cpp \

HEADERS += \

include(../../lib/use.pri)
