import qbs

CppApplication {
    consoleApplication: true
    condition: protobuf.present && qbs.targetOS == qbs.hostOS

    Depends { name: "cpp" }
    cpp.cxxLanguageVersion: "c++11"
    cpp.minimumMacosVersion: "10.8"
    cpp.warningLevel: "none"

    Depends { id: protobuf; name: "protobuf.cpp"; required: false }

    files: [
        "../shared/addressbook.proto",
        "main.cpp",
        "README.md",
    ]
}
