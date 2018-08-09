import qbs

CppApplication {
    name: "addressbook_cpp"
    consoleApplication: true
    condition: protobuf.present

    Depends { name: "cpp" }
    cpp.cxxLanguageVersion: "c++11"

    Depends { id: protobuf; name: "protobuf.cpp"; required: false }
    Probe {
        id: presentProbe
        property bool hasModule: protobuf.present
        configure: {
            console.info("has protobuf: " + hasModule);
        }
    }

    files: [
        "addressbook.proto",
        "main.cpp",
    ]
}
