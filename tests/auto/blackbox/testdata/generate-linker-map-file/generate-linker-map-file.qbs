Project {
    CppApplication {
        name: "app-map"
        files: ["main.cpp"]
        // lld-link has different flag for map files, test it by switching to "lld" linkerVariant
        Properties { condition: qbs.toolchain.contains("clang-cl"); cpp.linkerVariant: "lld" }
        cpp.generateLinkerMapFile: true
    }
    CppApplication {
        name: "app-nomap"
        files: ["main.cpp"]
        Properties { condition: qbs.toolchain.contains("clang-cl"); cpp.linkerVariant: "lld" }
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
