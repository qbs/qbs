TARGET = tst_blackbox-windows

HEADERS = tst_blackboxwindows.h tst_blackboxbase.h
SOURCES = tst_blackboxwindows.cpp tst_blackboxbase.cpp
OBJECTS_DIR = windows
MOC_DIR = $${OBJECTS_DIR}-moc

include(../auto.pri)

DATA_DIRS = testdata-windows ../find

for(data_dir, DATA_DIRS) {
    files = $$files($$PWD/$$data_dir/*, true)
    win32:files ~= s|\\\\|/|g
    for(file, files):!exists($$file/*):FILES += $$file
}

OTHER_FILES += $$FILES
