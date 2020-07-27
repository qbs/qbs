CppApplication {
    Depends { name: "capnproto.cpp"; required: false }
    condition: capnproto.cpp.present && qbs.targetPlatform === qbs.hostPlatform
    consoleApplication: true
    cpp.minimumMacosVersion: "10.8"

    files: [
        "addressbook.capnp",
        "addressbook.cpp"
    ]
}
