import qbs

CppApplication {
    name: "app"
    property bool escapeLinkerFlags
    Properties {
        condition: escapeLinkerFlags
        cpp.linkerFlags: ["-Wl,-z,defs"]
    }
    Properties {
        condition: !escapeLinkerFlags
        cpp.linkerFlags: ["-z", "defs"]
    }
    files: ["main.cpp"]
}
