QT = core gui
greaterThan(QT_MAJOR_VERSION, 4):QT += widgets

TARGET = qbs-config-ui
DESTDIR = ../../../bin

HEADERS = settingsmodel.h mainwindow.h
SOURCES = settingsmodel.cpp mainwindow.cpp main.cpp
FORMS = mainwindow.ui

include(../../lib/use.pri)
