TARGET = tst_buildgraph

SOURCES = tst_buildgraph.cpp
HEADERS = tst_buildgraph.h

include(../auto.pri)
include(../../../src/app/shared/logging/logging.pri)
include(../../../src/lib/bundledlibs.pri)

qbs_use_bundled_qtscript {
    CONFIG += qbs_do_not_link_bundled_qtscript
    include(../../../src/lib/scriptengine/use_scriptengine.pri)
}
