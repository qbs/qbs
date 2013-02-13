HEADERS += \
    $$PWD/internaljobs.h \
    $$PWD/projectdata.h \
    $$PWD/runenvironment.h \
    $$PWD/jobs.h \
    $$PWD/project.h

SOURCES += \
    $$PWD/internaljobs.cpp \
    $$PWD/runenvironment.cpp \
    $$PWD/projectdata.cpp \
    $$PWD/jobs.cpp \
    $$PWD/project.cpp

api_headers.files = $$PWD/projectdata.h $$PWD/runenvironment.h $$PWD/jobs.h $$PWD/project.h
api_headers.path = /include/qbs/api
INSTALLS += api_headers
