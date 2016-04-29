import qbs

CppApplication {
    Depends { name: "Qt.dbus" }
    Group {
        files: ["..."]
        fileTags: ["qt.dbus.adaptor"]
    }
}
