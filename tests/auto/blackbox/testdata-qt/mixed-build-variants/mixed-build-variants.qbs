Project {
    Probe {
        id: tcProbe
        property bool isMsvc: qbs.toolchain.contains("msvc")
        configure: { console.info("is msvc: " + isMsvc); }
    }
    QtApplication {
        Properties {
            condition: qbs.toolchain.includes("msvc")
            Qt.core.qtBuildVariant: "release"
            qbs.buildVariant: "debug"
        }
        Qt.core.qtBuildVariant: "dummy"
        files: ["main.cpp"]
    }
}
