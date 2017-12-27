include(../../library_dirname.pri)

isEmpty(QBSLIBDIR) {
    QBSLIBDIR = $${OUT_PWD}/../../../$${QBS_LIBRARY_DIRNAME}
}

LIBNAME=qbsmsbuild

unix {
    LIBS += -L$${QBSLIBDIR} -l$${LIBNAME}
}

win32 {
    CONFIG(debug, debug|release) {
        QBSMSBUILDLIB = $${LIBNAME}d
    }
    CONFIG(release, debug|release) {
        QBSMSBUILDLIB = $${LIBNAME}
    }
    msvc {
        LIBS += /LIBPATH:$$QBSLIBDIR
        QBSMSBUILDLIB = $${QBSMSBUILDLIB}.lib
        LIBS += Shell32.lib
    } else {
        LIBS += -L$${QBSLIBDIR}
        QBSMSBUILDLIB = lib$${QBSMSBUILDLIB}
    }
    LIBS += $${QBSMSBUILDLIB}
}

INCLUDEPATH += \
    $$PWD

CONFIG += depend_includepath

CONFIG(static, static|shared) {
    DEFINES += QBS_STATIC_LIB
}
