HEADERS += \
    $$PWD/changeset.h \
    $$PWD/internaljobs.h \
    $$PWD/projectdata.h \
    $$PWD/projectfileupdater.h \
    $$PWD/runenvironment.h \
    $$PWD/jobs.h \
    $$PWD/languageinfo.h \
    $$PWD/project.h \
    $$PWD/propertymap_p.h \
    $$PWD/projectdata_p.h \
    $$PWD/qmljsrewriter.h

SOURCES += \
    $$PWD/changeset.cpp \
    $$PWD/internaljobs.cpp \
    $$PWD/projectfileupdater.cpp \
    $$PWD/runenvironment.cpp \
    $$PWD/projectdata.cpp \
    $$PWD/jobs.cpp \
    $$PWD/languageinfo.cpp \
    $$PWD/project.cpp \
    $$PWD/qmljsrewriter.cpp

!qbs_no_dev_install {
    api_headers.files = \
        $$PWD/jobs.h \
        $$PWD/languageinfo.h \
        $$PWD/project.h \
        $$PWD/projectdata.h \
        $$PWD/runenvironment.h
    api_headers.path = $${QBS_INSTALL_PREFIX}/include/qbs/api
    INSTALLS += api_headers
}
