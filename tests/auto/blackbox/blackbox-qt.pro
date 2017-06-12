TARGET = tst_blackbox-qt

HEADERS = tst_blackboxqt.h tst_blackboxbase.h
SOURCES = tst_blackboxqt.cpp tst_blackboxbase.cpp
OBJECTS_DIR = qt
MOC_DIR = $${OBJECTS_DIR}-moc

include(../auto.pri)

DATA_DIRS = testdata-qt ../find

for(data_dir, DATA_DIRS) {
    files = $$files($$PWD/$$data_dir/*, true)
    win32:files ~= s|\\\\|/|g
    for(file, files):!exists($$file/*):FILES += $$file
}

OTHER_FILES += $$FILES
