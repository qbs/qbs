import qbs

CppApplication {
    name: "addressbook_cpp"
    consoleApplication: true
    condition: protobuf.present

    Depends { name: "cpp" }
    cpp.cxxLanguageVersion: "c++11"

    Depends { id: protobuf; name: "protobuf.cpp"; required: false }

    files: [
        "../shared/addressbook.proto",
        "main.cpp",
        "README.md",
    ]
}
