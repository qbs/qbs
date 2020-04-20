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

    property path theImportDir
    protobuf.cpp.importPaths: (theImportDir ? [theImportDir] : []).concat([sourceDirectory])

    cpp.cxxLanguageVersion: "c++11"
    cpp.minimumMacosVersion: "10.8"

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
