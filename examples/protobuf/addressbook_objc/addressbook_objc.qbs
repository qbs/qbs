import qbs

CppApplication {
    consoleApplication: true
    condition: protobuf.present && qbs.targetOS.contains("darwin")

    Depends { name: "cpp" }
    Depends { id: protobuf; name: "protobuf.objc"; required: false }

    files: [
        "../shared/addressbook.proto",
        "main.m",
    ]
}
