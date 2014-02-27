!isEmpty(QBS_PLUGINS_BUILD_DIR) {
    destdirPrefix = $${QBS_PLUGINS_BUILD_DIR}
} else {
    greaterThan(QT_MAJOR_VERSION, 4): \
        destdirPrefix = $$shadowed($$PWD)/../../lib
    else: \
        destdirPrefix = ../../../../lib # Note: Will break if pri file is included from some other place
}
DESTDIR = $${destdirPrefix}/qbs/plugins
TEMPLATE = lib

CONFIG += depend_includepath
CONFIG += shared
unix: CONFIG += plugin

include(../library_dirname.pri)

!isEmpty(QBS_PLUGINS_INSTALL_DIR): \
    installPrefix = $${QBS_PLUGINS_INSTALL_DIR}
else: \
    installPrefix = $${QBS_INSTALL_PREFIX}/$${QBS_LIBRARY_DIRNAME}
target.path = $${installPrefix}/qbs/plugins
INSTALLS += target
