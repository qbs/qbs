include(../../../install_prefix.pri)

HEADERS += \
    $$PWD/internaljobs.h \
    $$PWD/projectdata.h \
    $$PWD/runenvironment.h \
    $$PWD/jobs.h \
    $$PWD/languageinfo.h \
    $$PWD/project.h \
    $$PWD/project_p.h \
    $$PWD/propertymap_p.h \
    $$PWD/projectdata_p.h \
    $$PWD/rulecommand.h \
    $$PWD/rulecommand_p.h \
    $$PWD/transformerdata.h \
    $$PWD/transformerdata_p.h

SOURCES += \
    $$PWD/internaljobs.cpp \
    $$PWD/runenvironment.cpp \
    $$PWD/projectdata.cpp \
    $$PWD/jobs.cpp \
    $$PWD/languageinfo.cpp \
    $$PWD/project.cpp \
    $$PWD/rulecommand.cpp \
    $$PWD/transformerdata.cpp

!qbs_no_dev_install {
    api_headers.files = \
        $$PWD/jobs.h \
        $$PWD/languageinfo.h \
        $$PWD/project.h \
        $$PWD/projectdata.h \
        $$PWD/rulecommand.h \
        $$PWD/runenvironment.h \
        $$PWD/transformerdata.h
    api_headers.path = $${QBS_INSTALL_PREFIX}/include/qbs/api
    INSTALLS += api_headers
}

qbs_enable_project_file_updates {
    HEADERS += \
        $$PWD/changeset.h \
        $$PWD/projectfileupdater.h \
        $$PWD/qmljsrewriter.h

    SOURCES += \
        $$PWD/changeset.cpp \
        $$PWD/projectfileupdater.cpp \
        $$PWD/qmljsrewriter.cpp
    DEFINES += QBS_ENABLE_PROJECT_FILE_UPDATES
}
