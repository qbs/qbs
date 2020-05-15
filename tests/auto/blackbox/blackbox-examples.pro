TARGET = tst_blackbox-examples

HEADERS = tst_blackboxexamples.h tst_blackboxbase.h
SOURCES = tst_blackboxexamples.cpp tst_blackboxbase.cpp
OBJECTS_DIR = examples
MOC_DIR = $${OBJECTS_DIR}-moc

include(../auto.pri)

DATA_DIRS = ../../../examples

for(data_dir, DATA_DIRS) {
    files = $$files($$PWD/$$data_dir/*, true)
    win32:files ~= s|\\\\|/|g
    for(file, files):!exists($$file/*):FILES += $$file
}

OTHER_FILES += $$FILES
