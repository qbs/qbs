include(../library_dirname.pri)
include(../install_prefix.pri)

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
DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_PROCESS_COMBINED_ARGUMENT_START
qbs_enable_unit_tests:DEFINES += QBS_ENABLE_UNIT_TESTS
INCLUDEPATH += $${PWD}/../
contains(QT_CONFIG, reduce_exports):CONFIG += hide_symbols
win32:CONFIG(debug, debug|release):TARGET = $${TARGET}d
CONFIG(debug, debug|release):DEFINES += QT_STRICT_ITERATORS
CONFIG += c++11
CONFIG += create_prl

qbs_disable_rpath {
    osx:QMAKE_LFLAGS_SONAME = -Wl,-install_name,$$QBS_INSTALL_PREFIX/$$QBS_LIBRARY_DIRNAME/
} else {
    osx:QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/
}
include(../../qbs_version.pri)
VERSION = $${QBS_VERSION}

linux {
    # Turn off absurd qmake's soname "logic" and directly add the linker flag.
    QMAKE_LFLAGS_SONAME =
    QMAKE_LFLAGS = -Wl,-soname=lib$${TARGET}.so.$${QBS_VERSION_MAJ}.$${QBS_VERSION_MIN}
}

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
