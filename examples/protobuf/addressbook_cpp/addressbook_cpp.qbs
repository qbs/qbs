import qbs.Host

CppApplication {
    consoleApplication: true
    condition: protobuf.cpp.present && qbs.targetPlatform === Host.platform()

    Depends { name: "cpp" }
    cpp.minimumMacosVersion: "10.8"

    Depends { name: "protobuf.cpp"; required: false }

    files: [
        "../shared/addressbook.proto",
        "main.cpp",
        "README.md",
    ]
    qbsModuleProviders: "qbspkgconfig"
}
