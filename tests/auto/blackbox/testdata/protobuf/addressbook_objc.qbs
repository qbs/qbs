import qbs.Host

CppApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch");
        return result && hasProtobuf;
    }
    name: "addressbook_objc"
    consoleApplication: true

    Depends { name: "cpp" }
    Depends { name: "protobuf.objc"; required: false }
    property bool hasProtobuf: {
        console.info("has protobuf: " + protobuf.objc.present);
        console.info("has modules: false");
        return protobuf.objc.present;
    }

    files: [
        "addressbook.proto",
        "main.m",
    ]
}
