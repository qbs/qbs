TEMPLATE = app
DESTDIR = ../../../bin
DEFINES += SRCDIR=\\\"$$_PRO_FILE_PWD_\\\"
qbs_test_suite_name = $$replace(_PRO_FILE_, ^.*/([^/.]+)\\.pro$, \\1)
qbs_test_suite_name = $$upper($$replace(qbs_test_suite_name, -, _))
DEFINES += QBS_TEST_SUITE_NAME=\\\"$${qbs_test_suite_name}\\\"
INCLUDEPATH += $$PWD/../../src $$PWD/../../src/app/shared

QT = core testlib
CONFIG += testcase console
CONFIG -= app_bundle
CONFIG += c++14
target.CONFIG += no_default_install

dev_lib_frameworks=$$QMAKE_XCODE_DEVELOPER_PATH/Library/Frameworks
exists($$dev_lib_frameworks): LIBS += -F$$dev_lib_frameworks

include(../../src/lib/corelib/use_corelib.pri)
