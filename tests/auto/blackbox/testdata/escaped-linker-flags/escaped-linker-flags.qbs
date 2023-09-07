CppApplication {
    name: "app"
    property bool escapeLinkerFlags
    Properties {
        condition: escapeLinkerFlags
        cpp.linkerFlags: ["-Wl,-s"]
    }
    Properties {
        condition: !escapeLinkerFlags
        cpp.linkerFlags: ["-s"]
    }
    files: ["main.cpp"]
    Probe {
        id: checker
        property bool isUnixGcc: qbs.toolchain.contains("gcc") && !qbs.targetOS.contains("macos")
        configure: { console.info("is gcc: " + isUnixGcc); }
    }
}
