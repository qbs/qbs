TEMPLATE = app
DESTDIR = ../../../bin
DEFINES += SRCDIR=\\\"$$_PRO_FILE_PWD_\\\"
INCLUDEPATH += $$PWD/../../src

QT = core script testlib
CONFIG += depend_includepath testcase console
CONFIG -= app_bundle
target.CONFIG += no_default_install

include(../../src/lib/use.pri)
