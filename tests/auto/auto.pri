TEMPLATE = app
DESTDIR = ../../../bin
DEFINES += SRCDIR=\\\"$$_PRO_FILE_PWD_/\\\"

QT = core script testlib
CONFIG += depend_includepath testcase console
CONFIG -= app_bundle

include(../../src/lib/use.pri)
