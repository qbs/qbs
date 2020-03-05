import qbs.Utilities

CppApplication {
    name: "myapp"
    Depends { name: "Qt.qml" }

    Qt.qml.importVersion: "1"
    cpp.includePaths: sourceDirectory
    qbs.installPrefix: ""

    files: [
        "main.cpp",
        "person.cpp",
        "person.h",
    ]

    Group {
        files: "example.qml"
        fileTags: "qt.core.resource_data"
    }

    Probe {
        id: versionProbe
        property string version: Qt.core.version
        configure: {
            if (Utilities.versionCompare(version, "5.15") >= 0)
                console.info("has registrar");
            else
                console.info("does not have registrar");
            found = true;
        }
    }
}
