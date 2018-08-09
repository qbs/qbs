import qbs

CppApplication {
    name: "app"
    consoleApplication: true
    condition: protobuf.cpp.present

    protobuf.cpp.importPaths: [sourceDirectory]

    cpp.cxxLanguageVersion: "c++11"

    Depends { name: "protobuf.cpp"; required: false }
    Probe {
        id: presentProbe
        property bool hasModule: protobuf.cpp.present
        configure: {
            console.info("has protobuf: " + hasModule);
        }
    }

    files: [
        "import.proto",
        "import-main.cpp",
        "subdir/myenum.proto",
    ]
}
