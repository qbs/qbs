requires(!cross_compile)
include(src/lib/bundledlibs.pri)

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

!minQtVersion(5, 11, 0) {
    message("Cannot build qbs with Qt version $${QT_VERSION}.")
    error("Use at least Qt 5.11.0.")
}

TEMPLATE = subdirs
corelib.file = src/lib/corelib/corelib.pro
msbuildlib.subdir = src/lib/msbuild
msbuildlib.depends = corelib
src_app.subdir = src/app
src_app.depends = corelib
src_libexec.subdir = src/libexec
src_plugins.subdir = src/plugins
CONFIG(shared, static|shared) {
    src_plugins.depends = corelib
    src_app.depends += src_plugins
}
src_plugins.depends += msbuildlib
tests.depends = static_res
static_res.file = static-res.pro
static_res.depends = src_app src_libexec src_plugins static.pro
qbs_use_bundled_qtscript {
    scriptenginelib.file = src/lib/scriptengine/scriptengine.pro
    corelib.depends = scriptenginelib
    SUBDIRS += scriptenginelib
}
SUBDIRS += \
    corelib\
    msbuildlib\
    src_app\
    src_libexec\
    src_plugins\
    static.pro\
    static_res\
    tests

OTHER_FILES += \
    doc/*.qdoc \
    doc/reference/*.qdoc \
    doc/reference/items/convenience/*.qdoc \
    doc/reference/items/language/*.qdoc \
    doc/reference/jsextensions/*.qdoc \
    doc/reference/modules/*.qdoc \
    doc/targets/*.qdoc* \
    doc/qbs.qdocconf \
    doc/config/qbs-project.qdocconf

include(qbs_version.pri)
include(doc/doc.pri)
