HEADERS += \
    $$PWD/internaljobs.h \
    $$PWD/projectdata.h \
    $$PWD/runenvironment.h \
    $$PWD/jobs.h \
    $$PWD/project.h \
    $$PWD/propertymap_p.h \
    $$PWD/projectdata_p.h

SOURCES += \
    $$PWD/internaljobs.cpp \
    $$PWD/runenvironment.cpp \
    $$PWD/projectdata.cpp \
    $$PWD/jobs.cpp \
    $$PWD/project.cpp

!qbs_no_dev_install {
    api_headers.files = $$PWD/projectdata.h $$PWD/runenvironment.h $$PWD/jobs.h $$PWD/project.h
    api_headers.path = $${QBS_INSTALL_PREFIX}/include/qbs/api
    INSTALLS += api_headers
}
