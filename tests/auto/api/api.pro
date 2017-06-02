TARGET = tst_api

HEADERS = tst_api.h
SOURCES = tst_api.cpp

include(../../../src/library_dirname.pri)
isEmpty(QBS_RELATIVE_LIBEXEC_PATH) {
    win32:QBS_RELATIVE_LIBEXEC_PATH=.
    else:QBS_RELATIVE_LIBEXEC_PATH=../libexec/qbs
}
isEmpty(QBS_RELATIVE_PLUGINS_PATH):QBS_RELATIVE_PLUGINS_PATH=../$${QBS_LIBRARY_DIRNAME}
isEmpty(QBS_RELATIVE_SEARCH_PATH):QBS_RELATIVE_SEARCH_PATH=..
DEFINES += QBS_RELATIVE_LIBEXEC_PATH=\\\"$${QBS_RELATIVE_LIBEXEC_PATH}\\\"
DEFINES += QBS_RELATIVE_PLUGINS_PATH=\\\"$${QBS_RELATIVE_PLUGINS_PATH}\\\"
DEFINES += QBS_RELATIVE_SEARCH_PATH=\\\"$${QBS_RELATIVE_SEARCH_PATH}\\\"
qbs_enable_project_file_updates:DEFINES += QBS_ENABLE_PROJECT_FILE_UPDATES

include(../auto.pri)

DATA_DIRS = testdata

for(data_dir, DATA_DIRS) {
    files = $$files($$PWD/$$data_dir/*, true)
    win32:files ~= s|\\\\|/|g
    for(file, files):!exists($$file/*):FILES += $$file
}

OTHER_FILES += $$FILES
