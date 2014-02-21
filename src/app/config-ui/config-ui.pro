include(../app.pri)

CONFIG -= console
QT += gui
greaterThan(QT_MAJOR_VERSION, 4):QT += widgets

TARGET = qbs-config-ui

HEADERS += commandlineparser.h settingsmodel.h mainwindow.h
SOURCES += commandlineparser.cpp settingsmodel.cpp mainwindow.cpp main.cpp
FORMS += mainwindow.ui
