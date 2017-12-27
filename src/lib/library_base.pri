include(../library_dirname.pri)
include(../install_prefix.pri)

TEMPLATE = lib
QT = core
!isEmpty(QBS_DLLDESTDIR):DLLDESTDIR = $${QBS_DLLDESTDIR}
else:DLLDESTDIR = ../../../bin
!isEmpty(QBS_DESTDIR):DESTDIR = $${QBS_DESTDIR}
else:DESTDIR = ../../../$${QBS_LIBRARY_DIRNAME}

DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_PROCESS_COMBINED_ARGUMENT_START
qbs_enable_unit_tests:DEFINES += QBS_ENABLE_UNIT_TESTS
INCLUDEPATH += $${PWD}/../
contains(QT_CONFIG, reduce_exports):CONFIG += hide_symbols
win32:CONFIG(debug, debug|release):TARGET = $${TARGET}d
CONFIG(debug, debug|release):DEFINES += QT_STRICT_ITERATORS
CONFIG += c++14
CONFIG += create_prl

include(../../qbs_version.pri)
VERSION = $${QBS_VERSION}
