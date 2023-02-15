QtApplication {
    Properties {
        condition: qbs.toolchain.includes("msvc")
        Qt.core.qtBuildVariant: "release"
    }
    Qt.core.qtBuildVariant: "dummy"

    files: ["main.cpp"]
}
