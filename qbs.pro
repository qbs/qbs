defineTest(minQtVersion) {
    maj = $$1
    min = $$2
    patch = $$3
    isEqual(QT_MAJOR_VERSION, $$maj) {
        isEqual(QT_MINOR_VERSION, $$min) {
            isEqual(QT_PATCH_VERSION, $$patch) {
                return(true)
            }
            greaterThan(QT_PATCH_VERSION, $$patch) {
                return(true)
            }
        }
        greaterThan(QT_MINOR_VERSION, $$min) {
            return(true)
        }
    }
    greaterThan(QT_MAJOR_VERSION, $$maj) {
        return(true)
    }
    return(false)
}

!minQtVersion(4, 8, 0) {
    message("Cannot build qbs with Qt version $${QT_VERSION}.")
    error("Use at least Qt 4.8.0.")
}

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
    doc/*.qdoc \
    doc/items/*.qdoc \
    doc/qbs.qdocconf \
    doc/config/qbs-project.qdocconf

include(qbs_version.pri)
include(doc/doc.pri)
