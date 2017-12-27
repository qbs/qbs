include(library_base.pri)

CONFIG(static, static|shared) {
    DEFINES += QBS_STATIC_LIB
} else {
    DEFINES += QBS_LIBRARY
}

qbs_disable_rpath {
    osx:QMAKE_LFLAGS_SONAME = -Wl,-install_name,$$QBS_INSTALL_PREFIX/$$QBS_LIBRARY_DIRNAME/
} else {
    osx:QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/
}

linux {
    # Turn off absurd qmake's soname "logic" and directly add the linker flag.
    QMAKE_LFLAGS_SONAME =
    QMAKE_LFLAGS += -Wl,-soname=lib$${TARGET}.so.$${QBS_VERSION_MAJ}.$${QBS_VERSION_MIN}
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
