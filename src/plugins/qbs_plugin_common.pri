include(../library_dirname.pri)
include(../install_prefix.pri)

!isEmpty(QBS_PLUGINS_BUILD_DIR) {
    destdirPrefix = $${QBS_PLUGINS_BUILD_DIR}
} else {
    destdirPrefix = $$shadowed($$PWD)/../../$${QBS_LIBRARY_DIRNAME}
}
qbsPluginDestDir = $${destdirPrefix}/qbs/plugins
