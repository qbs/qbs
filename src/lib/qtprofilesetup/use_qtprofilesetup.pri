include(../../../qbs_version.pri)
include(../../library_dirname.pri)

isEmpty(QBSLIBDIR) {
    QBSLIBDIR = $${OUT_PWD}/../../../$${QBS_LIBRARY_DIRNAME}
}

LIBNAME=qbsqtprofilesetup

unix {
    LIBS += -L$${QBSLIBDIR} -l$${LIBNAME}
}

!qbs_disable_rpath {
    linux-*:QMAKE_LFLAGS += -Wl,-z,origin \'-Wl,-rpath,\$\$ORIGIN/../$${QBS_LIBRARY_DIRNAME}\'
    macx:QMAKE_LFLAGS += -Wl,-rpath,@loader_path/../$${QBS_LIBRARY_DIRNAME}
}

!CONFIG(static, static|shared) {
    QBSQTPROFILELIBSUFFIX = $${QBS_VERSION_MAJ}
}

win32 {
    CONFIG(debug, debug|release) {
        QBSQTPROFILELIB = $${LIBNAME}d$${QBSQTPROFILELIBSUFFIX}
    }
    CONFIG(release, debug|release) {
        QBSQTPROFILELIB = $${LIBNAME}$${QBSQTPROFILELIBSUFFIX}
    }
    msvc {
        LIBS += /LIBPATH:$$QBSLIBDIR
        QBSQTPROFILELIB = $${QBSQTPROFILELIB}.lib
        LIBS += Shell32.lib
    } else {
        LIBS += -L$${QBSLIBDIR}
        QBSQTPROFILELIB = lib$${QBSQTPROFILELIB}
    }
    LIBS += $${QBSQTPROFILELIB}
}

INCLUDEPATH += \
    $$PWD

CONFIG += depend_includepath

CONFIG(static, static|shared) {
    DEFINES += QBS_STATIC_LIB
}
