DESTDIR= ../../../../plugins
TEMPLATE = lib
TARGET = qbs_qt_scanner
DEPENDPATH += .
INCLUDEPATH += .

Qt = core xml

unix: CONFIG += plugin

HEADERS += ../scanner.h
SOURCES += qt.cpp
