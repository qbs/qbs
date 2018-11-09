QtGuiApplication {
    Probe {
        id: staticProbe
        property bool isStaticQt: Qt.gui.isStaticLibrary
        property var plugins: Qt.plugin_support.effectivePluginsByType
        property var allPlugins: Qt.plugin_support.allPluginsByType
        configure: {
            console.info("static Qt: " + isStaticQt);
            console.info("requested image plugins: %" + plugins.imageformats + "%");
            console.info("all image plugins: #" + allPlugins.imageformats + "#");
            console.info("platform plugin count: " + (plugins.platforms || []).length);
        }
    }

    Depends { name: "m1" }
    Depends { name: "m2" }
    files: "plugin-support-main.cpp"
}
