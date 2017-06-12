TARGET = tst_blackbox-clangdb

HEADERS = tst_blackboxbase.h tst_clangdb.h
SOURCES = tst_blackboxbase.cpp tst_clangdb.cpp
OBJECTS_DIR = clangdb
MOC_DIR = $${OBJECTS_DIR}-moc

include(../auto.pri)

DATA_DIRS = testdata-clangdb

for(data_dir, DATA_DIRS) {
    files = $$files($$PWD/$$data_dir/*, true)
    win32:files ~= s|\\\\|/|g
    for(file, files):!exists($$file/*):FILES += $$file
}

OTHER_FILES += $$FILES
