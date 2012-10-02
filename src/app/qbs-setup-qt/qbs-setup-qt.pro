QT       += core
QT       -= gui

TARGET = qbs-setup-qt
CONFIG   += console
CONFIG   -= app_bundle
DESTDIR = ../../../bin

TEMPLATE = app

SOURCES += \
    main.cpp \
    setupqt.cpp

HEADERS += \
    setupqt.h
