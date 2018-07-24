TARGET = tst_blackbox-joblimits

HEADERS = tst_blackboxbase.h
SOURCES = tst_blackboxjoblimits.cpp tst_blackboxbase.cpp
OBJECTS_DIR = joblimits
MOC_DIR = $${OBJECTS_DIR}-moc

include(../auto.pri)

DATA_DIRS = testdata-joblimits ../find

for(data_dir, DATA_DIRS) {
    files = $$files($$PWD/$$data_dir/*, true)
    win32:files ~= s|\\\\|/|g
    for(file, files):!exists($$file/*):FILES += $$file
}

OTHER_FILES += $$FILES
