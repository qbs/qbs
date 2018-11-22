import qbs

CppApplication {
    name: "addressbook_objc"
    consoleApplication: true
    condition: hasProtobuf

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
