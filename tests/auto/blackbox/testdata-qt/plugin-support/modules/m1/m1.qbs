Module {
    property bool useDummy
    Depends { name: "Qt.plugin_support" }
    Properties {
        condition: useDummy
        Qt.plugin_support.pluginsByType: ({imageformats: "dummy"})
    }
    Properties {
        condition: Qt.plugin_support.allPluginsByType && Qt.plugin_support.allPluginsByType.imageformats
        Qt.plugin_support.pluginsByType: ({imageformats: Qt.plugin_support.allPluginsByType.imageformats[0]})
    }
}
