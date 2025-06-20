import qbs.Host

CppApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch");
        return result && hasProtobuf;
    }
    name: "addressbook_cpp"
    consoleApplication: true

    Depends { name: "cpp" }
    cpp.minimumMacosVersion: "10.15"

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
