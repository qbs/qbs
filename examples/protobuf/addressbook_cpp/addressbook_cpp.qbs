CppApplication {
    consoleApplication: true
    condition: protobuf.cpp.present && qbs.targetPlatform === qbs.hostPlatform

    Depends { name: "cpp" }
    cpp.cxxLanguageVersion: "c++11"
    cpp.minimumMacosVersion: "10.8"

    Depends { name: "protobuf.cpp"; required: false }

    files: [
        "../shared/addressbook.proto",
        "main.cpp",
        "README.md",
    ]
}
