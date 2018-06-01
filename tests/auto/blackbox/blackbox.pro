TARGET = tst_blackbox

HEADERS = tst_blackbox.h tst_blackboxbase.h
SOURCES = tst_blackbox.cpp tst_blackboxbase.cpp
OBJECTS_DIR = generic
MOC_DIR = $${OBJECTS_DIR}-moc
qbs_enable_unit_tests:DEFINES += QBS_ENABLE_UNIT_TESTS

include(../auto.pri)

QT += xml

DATA_DIRS = testdata ../find

for(data_dir, DATA_DIRS) {
    files = $$files($$PWD/$$data_dir/*, true)
    win32:files ~= s|\\\\|/|g
    for(file, files):!exists($$file/*):FILES += $$file
}

OTHER_FILES += $$FILES
