include(../library_dirname.pri)
include(../install_prefix.pri)

!isEmpty(QBS_PLUGINS_BUILD_DIR) {
    destdirPrefix = $${QBS_PLUGINS_BUILD_DIR}
} else {
    destdirPrefix = $$shadowed($$PWD)/../../$${QBS_LIBRARY_DIRNAME}
}
DESTDIR = $${destdirPrefix}/qbs/plugins
CONFIG(static, static|shared) {
    DEFINES += QBS_STATIC_LIB
} else {
    DEFINES += QBS_LIBRARY
}
TEMPLATE = lib

CONFIG += depend_includepath
CONFIG += c++11
CONFIG += create_prl
unix: CONFIG += plugin

!isEmpty(QBS_PLUGINS_INSTALL_DIR): \
    installPrefix = $${QBS_PLUGINS_INSTALL_DIR}
else: \
    installPrefix = $${QBS_INSTALL_PREFIX}/$${QBS_LIBRARY_DIRNAME}
target.path = $${installPrefix}/qbs/plugins
INSTALLS += target
