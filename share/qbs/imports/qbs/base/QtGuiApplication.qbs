import qbs

CppApplication {
    Depends { name: "Qt.gui" }
    Depends {
        name: "Qt"
        submodules: Qt.gui.defaultQpaPlugin
        condition: linkDefaultQpaPlugin && Qt.gui.defaultQpaPlugin
    }
    property bool linkDefaultQpaPlugin: Qt.gui.isStaticLibrary
}
