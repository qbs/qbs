QBS_VERSION = 0.2.0
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
    doc/qbs.qdoc

include(doc/doc.pri)
