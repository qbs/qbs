import qbs

CppApplication {
    name: "app"
    consoleApplication: true
    condition: protobuf.cpp.present

    property path theImportDir
    protobuf.cpp.importPaths: (theImportDir ? [theImportDir] : []).concat([sourceDirectory])

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
        "needs-import-dir.proto",
        "needs-import-dir-main.cpp",
        "subdir/myenum.proto",
    ]
}
