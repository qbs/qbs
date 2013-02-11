TARGET = tst_cmdlineparser

SOURCES = tst_cmdlineparser.cpp ../../../src/app/qbs/qbstool.cpp

include(../auto.pri)
include(../../../src/app/qbs/parser/parser.pri)
include(../../../src/app/shared/logging/logging.pri)
