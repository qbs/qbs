TARGET = tst_blackbox-baremetal

HEADERS = tst_blackboxbaremetal.h tst_blackboxbase.h
SOURCES = tst_blackboxbaremetal.cpp tst_blackboxbase.cpp
OBJECTS_DIR = baremetal
MOC_DIR = $${OBJECTS_DIR}-moc

include(../auto.pri)

DATA_DIRS = testdata-baremetal ../find

for(data_dir, DATA_DIRS) {
    files = $$files($$PWD/$$data_dir/*, true)
    win32:files ~= s|\\\\|/|g
    for(file, files):!exists($$file/*):FILES += $$file
}

OTHER_FILES += $$FILES
