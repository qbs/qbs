include(../library_dirname.pri)
include(../install_prefix.pri)

!isEmpty(QBS_PLUGINS_BUILD_DIR) {
    destdirPrefix = $${QBS_PLUGINS_BUILD_DIR}
} else {
    destdirPrefix = $$shadowed($$PWD)/../../$${QBS_LIBRARY_DIRNAME}
}
DESTDIR = $${destdirPrefix}/qbs/plugins

isEmpty(QBSLIBDIR): QBSLIBDIR = $$OUT_PWD/../../../../$${QBS_LIBRARY_DIRNAME}
isEmpty(QBS_RPATH): QBS_RPATH = ../..
include($${PWD}/../lib/corelib/use_corelib.pri)

TEMPLATE = lib

CONFIG += c++14
CONFIG += create_prl
unix: CONFIG += plugin

!isEmpty(QBS_PLUGINS_INSTALL_DIR): \
    installPrefix = $${QBS_PLUGINS_INSTALL_DIR}
else: \
    installPrefix = $${QBS_INSTALL_PREFIX}/$${QBS_LIBRARY_DIRNAME}
target.path = $${installPrefix}/qbs/plugins
INSTALLS += target
