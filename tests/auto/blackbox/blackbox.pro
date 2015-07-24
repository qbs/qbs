TARGET = tst_blackbox

HEADERS = tst_blackbox.h
SOURCES = tst_blackbox.cpp

include(../auto.pri)

DATA_DIRS = testdata

for(data_dir, DATA_DIRS) {
    files = $$files($$PWD/$$data_dir/*, true)
    win32:files ~= s|\\\\|/|g
    for(file, files):!exists($$file/*):FILES += $$file
}

OTHER_FILES += $$FILES
