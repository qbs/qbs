TARGET = tst_blackbox-java

HEADERS = tst_blackboxjava.h tst_blackboxbase.h
SOURCES = tst_blackboxjava.cpp tst_blackboxbase.cpp
OBJECTS_DIR = java
MOC_DIR = $${OBJECTS_DIR}-moc

include(../auto.pri)

DATA_DIRS = testdata-java ../find

for(data_dir, DATA_DIRS) {
    files = $$files($$PWD/$$data_dir/*, true)
    win32:files ~= s|\\\\|/|g
    for(file, files):!exists($$file/*):FILES += $$file
}

OTHER_FILES += $$FILES
