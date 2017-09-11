requires(!cross_compile)

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

!minQtVersion(5, 6, 0) {
    message("Cannot build qbs with Qt version $${QT_VERSION}.")
    error("Use at least Qt 5.6.0.")
}

TEMPLATE = subdirs
corelib.file = src/lib/corelib/corelib.pro
setupqtprofilelib.subdir = src/lib/qtprofilesetup
setupqtprofilelib.depends = corelib
src_app.subdir = src/app
src_app.depends = setupqtprofilelib
src_libexec.subdir = src/libexec
src_plugins.subdir = src/plugins
CONFIG(shared, static|shared): src_plugins.depends = corelib
tests.depends = corelib src_plugins
SUBDIRS += \
    corelib\
    setupqtprofilelib\
    src_app\
    src_libexec\
    src_plugins\
    static.pro\
    tests

OTHER_FILES += \
    doc/*.qdoc \
    doc/reference/*.qdoc \
    doc/reference/items/convenience/*.qdoc \
    doc/reference/items/language/*.qdoc \
    doc/reference/jsextensions/*.qdoc \
    doc/reference/modules/*.qdoc \
    doc/qbs.qdocconf \
    doc/config/qbs-project.qdocconf

include(qbs_version.pri)
include(doc/doc.pri)
