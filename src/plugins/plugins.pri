include(qbs_plugin_common.pri)

TARGET = $$qbsPluginTarget
DESTDIR = $$qbsPluginDestDir

isEmpty(QBSLIBDIR): QBSLIBDIR = $$OUT_PWD/../../../../$${QBS_LIBRARY_DIRNAME}
isEmpty(QBS_RPATH): QBS_RPATH = ../..
include($${PWD}/../lib/corelib/use_corelib.pri)

TEMPLATE = lib

CONFIG += c++14
CONFIG(static, static|shared): CONFIG += create_prl
CONFIG += plugin

!isEmpty(QBS_PLUGINS_INSTALL_DIR): \
    installPrefix = $${QBS_PLUGINS_INSTALL_DIR}
else: \
    installPrefix = $${QBS_INSTALL_PREFIX}/$${QBS_LIBRARY_DIRNAME}
target.path = $${installPrefix}/qbs/plugins
INSTALLS += target
