TARGET = tst_api

HEADERS = tst_api.h
SOURCES = tst_api.cpp

qbs_enable_project_file_updates:DEFINES += QBS_ENABLE_PROJECT_FILE_UPDATES

include(../auto.pri)
