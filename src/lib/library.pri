include(../library_dirname.pri)

TEMPLATE = lib
QT = core
!isEmpty(QBS_DLLDESTDIR):DLLDESTDIR = $${QBS_DLLDESTDIR}
else:DLLDESTDIR = ../../../bin
!isEmpty(QBS_DESTDIR):DESTDIR = $${QBS_DESTDIR}
else:DESTDIR = ../../../$${QBS_LIBRARY_DIRNAME}
CONFIG(static, static|shared) {
    DEFINES += QBS_STATIC_LIB
} else {
    DEFINES += QBS_LIBRARY
}
INCLUDEPATH += $${PWD}/../
contains(QT_CONFIG, reduce_exports):CONFIG += hide_symbols
win32:CONFIG(debug, debug|release):TARGET = $${TARGET}d

!disable_rpath {
    macx:QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/
}
include(../../qbs_version.pri)
VERSION = $${QBS_VERSION}
win32 {
    dlltarget.path = $${QBS_INSTALL_PREFIX}/bin
    INSTALLS += dlltarget
}

!win32|!qbs_no_dev_install {
    !isEmpty(QBS_LIB_INSTALL_DIR): \
        target.path = $${QBS_LIB_INSTALL_DIR}
    else: \
        target.path = $${QBS_INSTALL_PREFIX}/$${QBS_LIBRARY_DIRNAME}
    INSTALLS += target
}
