include(use_installed_corelib.pri)

LIBNAME=qbsqtprofilesetup

unix:LIBS += -l$${LIBNAME}

win32 {
    CONFIG(debug, debug|release) {
        QBSQTPROFILELIB = $${LIBNAME}d$${QBSCORELIBSUFFIX}
    }
    CONFIG(release, debug|release) {
        QBSQTPROFILELIB = $${LIBNAME}$${QBSCORELIBSUFFIX}
    }
    win32-msvc* {
        QBSQTPROFILELIB = $${QBSQTPROFILELIB}.lib
    } else {
        QBSQTPROFILELIB = lib$${QBSQTPROFILELIB}
    }
    LIBS += $${QBSQTPROFILELIB}
}
