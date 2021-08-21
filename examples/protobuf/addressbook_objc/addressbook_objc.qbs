CppApplication {
    consoleApplication: true
    condition: protobuf.objc.present && qbs.targetOS.contains("macos")

    Depends { name: "cpp" }
    Depends { name: "protobuf.objc"; required: false }

    Group {
        cpp.automaticReferenceCounting: true
        files: "main.m"
    }
    files: "../shared/addressbook.proto"
}
