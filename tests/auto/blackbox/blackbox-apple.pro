TARGET = tst_blackbox-apple

HEADERS = tst_blackboxapple.h tst_blackboxbase.h
SOURCES = tst_blackboxapple.cpp tst_blackboxbase.cpp
OBJECTS_DIR = apple
MOC_DIR = $${OBJECTS_DIR}-moc

include(../auto.pri)

QT += xml

DATA_DIRS = testdata-apple ../find

for(data_dir, DATA_DIRS) {
    files = $$files($$PWD/$$data_dir/*, true)
    win32:files ~= s|\\\\|/|g
    for(file, files):!exists($$file/*):FILES += $$file
}

OTHER_FILES += $$FILES
