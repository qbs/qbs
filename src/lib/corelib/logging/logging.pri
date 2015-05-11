include(../../../install_prefix.pri)

HEADERS += \
    $$PWD/logger.h \
    $$PWD/translator.h \
    $$PWD/ilogsink.h

SOURCES += \
    $$PWD/logger.cpp \
    $$PWD/ilogsink.cpp

!qbs_no_dev_install {
    logging_headers.files = $$PWD/ilogsink.h
    logging_headers.path = $${QBS_INSTALL_PREFIX}/include/qbs/logging
    INSTALLS += logging_headers
}
