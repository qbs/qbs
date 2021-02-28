//![0]

import qbs.Host

CppApplication {
    consoleApplication: true
    condition: protobuf.cpp.present && qbs.targetPlatform === Host.platform()

    Depends { name: "cpp" }
    cpp.minimumMacosVersion: "11.0"

    Depends { name: "protobuf.cpp"; required: false }

    files: [
        "../shared/addressbook.proto",
        "main.cpp",
    ]
    qbsModuleProviders: "conan"
}
//![0]
