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
}
