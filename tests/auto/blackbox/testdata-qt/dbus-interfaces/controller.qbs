CppApplication {
    name: "controller"
    condition: Qt.dbus.present
    Depends { name: "Qt.dbus"; required: false }
    Depends { name: "Qt.widgets" }
    cpp.cxxLanguageVersion: "c++11"
    files: [
        "controller.cpp",
        "controller.h",
        "controller.ui",
        "main.cpp",
    ]

    Group {
        name: "DBUS Interface"
        files: ["car.xml"]
        fileTags: ["qt.dbus.interface"]
    }
}
