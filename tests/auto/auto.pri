TEMPLATE = app
DESTDIR = ../../../bin
DEFINES += SRCDIR=\\\"$$_PRO_FILE_PWD_\\\"
INCLUDEPATH += $$PWD/../../src $$PWD/../../src/app/shared

QT = core testlib
CONFIG += depend_includepath testcase console
CONFIG -= app_bundle
CONFIG += c++11
target.CONFIG += no_default_install

dev_lib_frameworks=$$QMAKE_XCODE_DEVELOPER_PATH/Library/Frameworks
exists($$dev_lib_frameworks): LIBS += -F$$dev_lib_frameworks

include(../../src/lib/corelib/use_corelib.pri)
