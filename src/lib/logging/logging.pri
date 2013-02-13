HEADERS += \
    $$PWD/logger.h \
    $$PWD/translator.h \
    $$PWD/ilogsink.h

SOURCES += \
    $$PWD/logger.cpp \
    $$PWD/ilogsink.cpp

logging_headers.files = $$PWD/ilogsink.h
logging_headers.path = /include/qbs/logging
INSTALLS += logging_headers
