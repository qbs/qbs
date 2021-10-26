TARGET = qbspkgconfig
include(../staticlibrary.pri)
include(../../shared/variant/variant.pri)

DEFINES += \
    PKG_CONFIG_PC_PATH=\\\"/usr/lib/pkgconfig:/usr/share/pkgconfig\\\" \
    PKG_CONFIG_SYSTEM_LIBRARY_PATH=\\\"/usr/$${QBS_LIBRARY_DIRNAME}/\\\" \
    QBS_PC_WITH_QT_SUPPORT=1

macos|win32-g++ {
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

