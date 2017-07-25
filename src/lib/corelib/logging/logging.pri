include(../../../install_prefix.pri)

HEADERS += \
    $$PWD/categories.h \
    $$PWD/logger.h \
    $$PWD/translator.h \
    $$PWD/ilogsink.h

SOURCES += \
    $$PWD/categories.cpp \
    $$PWD/logger.cpp \
    $$PWD/ilogsink.cpp

!qbs_no_dev_install {
    logging_headers.files = $$PWD/ilogsink.h
    logging_headers.path = $${QBS_INSTALL_PREFIX}/include/qbs/logging
    INSTALLS += logging_headers
}
