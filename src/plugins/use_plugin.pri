include(qbs_plugin_common.pri)

qbsPluginLibName = $$qbsPluginTarget
win32:CONFIG(debug, debug|release):CONFIG(static, static|shared): \
    qbsPluginLibName = $${qbsPluginLibName}d
LIBS += -l$$qbsPluginLibName

macos: QMAKE_LFLAGS += -Wl,-u,_qbs_static_plugin_register_$$qbsPluginTarget
!macos:gcc: QMAKE_LFLAGS += -Wl,--require-defined=qbs_static_plugin_register_$$qbsPluginTarget
msvc: QMAKE_LFLAGS += /INCLUDE:qbs_static_plugin_register_$$qbsPluginTarget
