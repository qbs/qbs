import qbs

CppApplication {
    condition: {
        var result = qbs.targetPlatform === qbs.hostPlatform;
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result && hasProtobuf;
    }
    name: "addressbook_objc"
    consoleApplication: true

    Depends { name: "cpp" }
    Depends { name: "protobuf.objc"; required: false }
    property bool hasProtobuf: {
        console.info("has protobuf: " + protobuf.objc.present);
        return protobuf.objc.present;
    }

    files: [
        "addressbook.proto",
        "main.m",
    ]
}
