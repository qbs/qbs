QT = core script
all_tests:QT += testlib

TEMPLATE = lib
!isEmpty(QBS_DLLDESTDIR):DLLDESTDIR = $${QBS_DLLDESTDIR}
else:DLLDESTDIR = ../../bin
!isEmpty(QBS_DESTDIR):DESTDIR = $${QBS_DESTDIR}
else:DESTDIR = ../../lib
INCLUDEPATH += $$PWD
TARGET = qbscore

CONFIG += depend_includepath
DEFINES += QT_CREATOR QML_BUILD_STATIC_LIB      # needed for QmlJS

CONFIG(static, static|shared) {
    DEFINES += QBS_STATIC_LIB
} else {
    DEFINES += QBS_LIBRARY
}

DEFINES += SRCDIR=\\\"$$PWD\\\"
contains(QT_CONFIG, reduce_exports):CONFIG += hide_symbols

win32:CONFIG(debug, debug|release):TARGET = $${TARGET}d
win32-msvc*|win32-icc:QMAKE_CXXFLAGS += /WX
else:*g++*|*clang*|*icc*:QMAKE_CXXFLAGS += -Werror

!disable_rpath {
    macx:QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/
}

include(../../qbs_version.pri)
include(api/api.pri)
include(buildgraph/buildgraph.pri)
include(jsextensions/jsextensions.pri)
include(language/language.pri)
include(logging/logging.pri)
include(parser/parser.pri)
include(tools/tools.pri)

HEADERS += \
    qbs.h

win32 {
    dlltarget.path = $${QBS_INSTALL_PREFIX}/bin
    INSTALLS += dlltarget
}

!win32|!qbs_no_dev_install {
    !isEmpty(QBS_LIB_INSTALL_DIR): \
        target.path = $${QBS_LIB_INSTALL_DIR}
    else: \
        target.path = $${QBS_INSTALL_PREFIX}/lib
    INSTALLS += target
}

!qbs_no_dev_install {
    qbs_h.files = qbs.h
    qbs_h.path = $${QBS_INSTALL_PREFIX}/include/qbs
    INSTALLS += qbs_h
}
