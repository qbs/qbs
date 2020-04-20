import qbs

CppApplication {
    condition: {
        var result = qbs.targetPlatform === qbs.hostPlatform;
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result && hasProtobuf;
    }
    name: "app"
    consoleApplication: true

    protobuf.cpp.importPaths: [sourceDirectory]

    cpp.cxxLanguageVersion: "c++11"
    cpp.minimumMacosVersion: "10.8"

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
