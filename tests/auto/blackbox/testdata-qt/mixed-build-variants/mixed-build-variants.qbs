QtApplication {
    Properties {
        condition: qbs.toolchain.contains("msvc")
        Qt.core.qtBuildVariant: "release"
    }
    Qt.core.qtBuildVariant: "dummy"

    files: ["main.cpp"]
}
