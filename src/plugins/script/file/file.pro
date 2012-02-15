DESTDIR= ../../../../plugins/script/
TEMPLATE = lib
TARGET = qtscript_fileapi
DEPENDPATH += .
INCLUDEPATH += . ../../../lib/

QT = core script

unix: CONFIG += plugin

HEADERS += \
    file.h \
    ../../../lib/tools/fileinfo.h \
    textfile.h

SOURCES += \
    plugin.cpp \
    file.cpp \
    ../../../lib/tools/fileinfo.cpp \
    textfile.cpp
