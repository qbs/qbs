import qbs.Host

CppApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform();
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result && hasProtobuf;
    }
    name: "addressbook_cpp"
    consoleApplication: true

    Depends { name: "cpp" }
    cpp.minimumMacosVersion: "10.8"

    Depends { name: "protobuf.cpp"; required: false }
    property bool hasProtobuf: {
        console.info("has protobuf: " + protobuf.cpp.present);
        console.info("has modules: " + protobuflib.present);
        return protobuf.cpp.present;
    }

    files: [
        "addressbook.proto",
        "main.cpp",
    ]
}
