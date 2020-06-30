import qbs

CppApplication {
    consoleApplication: true
    condition: protobuf.objc.present && qbs.targetOS.contains("macos")

    Depends { name: "cpp" }
    Depends { name: "protobuf.objc"; required: false }

    files: [
        "../shared/addressbook.proto",
        "main.m",
    ]
}
