import qbs

CppApplication {
    name: "addressbook_cpp"
    consoleApplication: true
    condition: hasProtobuf

    Depends { name: "cpp" }
    cpp.cxxLanguageVersion: "c++11"

    Depends { name: "protobuf.cpp"; required: false }
    property bool hasProtobuf: {
        console.info("has protobuf: " + protobuf.cpp.present);
        return protobuf.cpp.present;
    }

    files: [
        "addressbook.proto",
        "main.cpp",
    ]
}
