import qbs

CppApplication {
    name: "car"
    condition: Qt.dbus.present
    Depends { name: "Qt.dbus"; required: false }
    Depends { name: "Qt.widgets" }
    files: [
        "car.cpp",
        "car.h",
        "main.cpp",
    ]

    Group {
        name: "DBUS Adaptor"
        files: ["THIS.IS.A.STRANGE.FILENAME.CAR.XML"]
        fileTags: ["qt.dbus.adaptor"]
    }
}
