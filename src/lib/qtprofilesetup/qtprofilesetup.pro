TARGET = qbsqtprofilesetup
include(../library.pri)
include(../corelib/use_corelib.pri)

HEADERS = \
    qtenvironment.h \
    qtmoduleinfo.h \
    qtprofilesetup.h

SOURCES = \
    qtmoduleinfo.cpp \
    qtprofilesetup.cpp

RESOURCES = templates.qrc

!qbs_no_dev_install {
    header.files = qtenvironment.h qtprofilesetup.h
    header.path = $${QBS_INSTALL_PREFIX}/include/qbs
    use_pri.files = use_installed_qtprofilesetup.pri
    use_pri.path = $${header.path}
    INSTALLS += header use_pri
}

OTHER_FILES += templates/*
