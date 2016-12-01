TARGET = tst_language

SOURCES = tst_language.cpp

include(../auto.pri)
include(../../../src/app/shared/logging/logging.pri)

DATA_DIRS = testdata

for(data_dir, DATA_DIRS) {
    files = $$files($$PWD/$$data_dir/*, true)
    win32:files ~= s|\\\\|/|g
    for(file, files):!exists($$file/*):FILES += $$file
}

OTHER_FILES += $$FILES
