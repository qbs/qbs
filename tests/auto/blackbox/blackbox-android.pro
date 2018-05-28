TARGET = tst_blackbox-android

HEADERS = tst_blackboxandroid.h tst_blackboxbase.h
SOURCES = tst_blackboxandroid.cpp tst_blackboxbase.cpp
OBJECTS_DIR = android
MOC_DIR = $${OBJECTS_DIR}-moc

include(../auto.pri)

DATA_DIRS = testdata-android ../find

for(data_dir, DATA_DIRS) {
    files = $$files($$PWD/$$data_dir/*, true)
    win32:files ~= s|\\\\|/|g
    for(file, files):!exists($$file/*):FILES += $$file
}

OTHER_FILES += $$FILES

DISTFILES += \
    testdata/texttemplate/expected-output-one.txt
