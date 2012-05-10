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
    doc/qbs.qdoc

include(qbs_version.pri)
include(doc/doc.pri)
