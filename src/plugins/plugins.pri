!isEmpty(QBS_RESOURCES_BUILD_DIR) {
    destdirPrefix = $${QBS_RESOURCES_BUILD_DIR}
} else {
    greaterThan(QT_MAJOR_VERSION, 4): \
        destdirPrefix = $$shadowed($$PWD)/../..
    else: \
        destdirPrefix = ../../../.. # Note: Will break if pri file is included from some other place
}
DESTDIR = $${destdirPrefix}/lib/qbs/plugins
TEMPLATE = lib

CONFIG += depend_includepath
CONFIG += shared
unix: CONFIG += plugin

!isEmpty(QBS_RESOURCES_INSTALL_DIR): \
    installPrefix = $${QBS_RESOURCES_INSTALL_DIR}
else: \
    installPrefix = $${QBS_INSTALL_PREFIX}
target.path = $${installPrefix}/lib/qbs/plugins
INSTALLS += target
