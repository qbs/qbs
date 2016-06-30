TARGET = tst_language

SOURCES = tst_language.cpp
HEADERS = tst_language.h

include(../auto.pri)
include(../../../src/app/shared/logging/logging.pri)
include(../../../src/lib/bundledlibs.pri)

!qbs_use_bundled_qtscript: QT += script

DATA_DIRS = testdata

for(data_dir, DATA_DIRS) {
    files = $$files($$PWD/$$data_dir/*, true)
    win32:files ~= s|\\\\|/|g
    for(file, files):!exists($$file/*):FILES += $$file
}

OTHER_FILES += $$FILES

qbs_use_bundled_qtscript {
    CONFIG += qbs_do_not_link_bundled_qtscript
    include(../../../src/lib/scriptengine/use_scriptengine.pri)
}
