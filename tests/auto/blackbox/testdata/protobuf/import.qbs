import qbs.Host

CppApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch");
        return result && hasProtobuf;
    }
    name: "app"
    consoleApplication: true

    protobuf.cpp.importPaths: [sourceDirectory]

    cpp.minimumMacosVersion: "10.8"

    Depends { name: "protobuf.cpp"; required: false }
    property bool hasProtobuf: {
        console.info("has protobuf: " + protobuf.cpp.present);
        console.info("has modules: " + protobuflib.present);
        return protobuf.cpp.present;
    }

    files: [
        "import.proto",
        "import-main.cpp",
        "subdir/myenum.proto",
    ]
}
