import qbs

CppApplication {
    Depends { name: "Qt.dbus"; required: false }
    Group {
        files: ["..."]
        fileTags: ["qt.dbus.adaptor"]
    }
}
