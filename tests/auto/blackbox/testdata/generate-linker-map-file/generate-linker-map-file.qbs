Project {
    CppApplication {
        name: "app-map"
        files: ["main.cpp"]
        cpp.generateLinkerMapFile: true
    }
    CppApplication {
        name: "app-nomap"
        files: ["main.cpp"]
        cpp.generateLinkerMapFile: false
    }
    CppApplication {
        name: "app-nomap-default"
        files: ["main.cpp"]
    }

    Probe {
        id: toolchainProbe
        property bool isUsed: qbs.toolchain.contains("msvc")
            || qbs.toolchain.contains("gcc")
        configure: {
            console.info("use test: " + isUsed);
        }
    }
}
