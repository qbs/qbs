import qbs

CppApplication {
    name: "app"
    consoleApplication: true
    condition: hasProtobuf

    protobuf.cpp.importPaths: [sourceDirectory]

    cpp.cxxLanguageVersion: "c++11"

    Depends { name: "protobuf.cpp"; required: false }
    property bool hasProtobuf: {
        console.info("has protobuf: " + protobuf.cpp.present);
        return protobuf.cpp.present;
    }

    files: [
        "import.proto",
        "import-main.cpp",
        "subdir/myenum.proto",
    ]
}
