TEMPLATE = app
DESTDIR = ../../../bin
DEFINES += SRCDIR=\\\"$$_PRO_FILE_PWD_\\\"
INCLUDEPATH += $$PWD/../../src

QT = core testlib
CONFIG += depend_includepath testcase console
CONFIG -= app_bundle
CONFIG += c++11
target.CONFIG += no_default_install

include(../../src/lib/corelib/use_corelib.pri)
