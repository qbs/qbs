include(use_installed_corelib.pri)

LIBNAME=qbsmsbuild

unix:LIBS += -l$${LIBNAME}

win32 {
    CONFIG(debug, debug|release) {
        QBSMSBUILDLIB = $${LIBNAME}d$${QBSCORELIBSUFFIX}
    }
    CONFIG(release, debug|release) {
        QBSMSBUILDLIB = $${LIBNAME}$${QBSCORELIBSUFFIX}
    }
    msvc {
        QBSMSBUILDLIB = $${QBSMSBUILDLIB}.lib
    } else {
        QBSMSBUILDLIB = lib$${QBSMSBUILDLIB}
    }
    LIBS += $${QBSMSBUILDLIB}
}
