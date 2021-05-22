TARGET = qbspkgconfig
include(../staticlibrary.pri)

DEFINES += \
    PKG_CONFIG_PC_PATH=\\\"/usr/lib/pkgconfig:/usr/share/pkgconfig\\\" \
    PKG_CONFIG_SYSTEM_LIBRARY_PATH=\\\"/usr/${QBS_LIBDIR_NAME}/\\\" \
    QBS_PC_WITH_QT_SUPPORT=1

macos {
    DEFINES += HAS_STD_FILESYSTEM=0
} else {
    DEFINES += HAS_STD_FILESYSTEM=1
}

HEADERS += \
    pcpackage.h \
    pcparser.h \
    pkgconfig.h

SOURCES += \
    pcpackage.cpp \
    pcparser.cpp \
    pkgconfig.cpp \

