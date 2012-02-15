QBS_VERSION = 0.1.0
TEMPLATE = subdirs
CONFIG += ordered
lib.file = src/lib/lib.pro
src_app.subdir = src/app
src_app.depends = lib
SUBDIRS += \
    lib\
    src_app\
    src/plugins\
    static.pro\
    tests

OTHER_FILES += \
    share/qbs/imports/qbs/base/* \
    share/qbs/imports/qbs/fileinfo/* \
    share/qbs/modules/*.* \
    share/qbs/modules/cpp/*.* \
    share/qbs/modules/qbs/*.* \
    share/qbs/modules/qt/*.* \
    share/qbs/modules/qt/core/*.* \
    share/qbs/modules/qt/gui/*.* \
    doc/qbs.qdoc

include(doc/doc.pri)
