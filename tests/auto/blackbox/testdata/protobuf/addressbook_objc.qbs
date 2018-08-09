import qbs

CppApplication {
    name: "addressbook_objc"
    consoleApplication: true
    condition: protobuf.present && qbs.targetOS.contains("darwin")

    Depends { name: "cpp" }
    Depends { id: protobuf; name: "protobuf.objc"; required: false }
    Probe {
        id: presentProbe
        property bool hasModule: protobuf.present && qbs.targetOS.contains("darwin")
        configure: {
            console.info("has protobuf: " + hasModule);
        }
    }

    files: [
        "addressbook.proto",
        "main.m",
    ]
}
