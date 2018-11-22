import qbs

CppApplication {
    name: "app"
    consoleApplication: true
    condition: hasProtobuf

    property path theImportDir
    protobuf.cpp.importPaths: (theImportDir ? [theImportDir] : []).concat([sourceDirectory])

    cpp.cxxLanguageVersion: "c++11"

    Depends { name: "protobuf.cpp"; required: false }
    property bool hasProtobuf: {
        console.info("has protobuf: " + protobuf.cpp.present);
        return protobuf.cpp.present;
    }

    files: [
        "needs-import-dir.proto",
        "needs-import-dir-main.cpp",
        "subdir/myenum.proto",
    ]
}
