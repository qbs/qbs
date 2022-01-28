import qbs.Host

CppApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform();
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result && hasProtobuf;
    }
    name: "addressbook_nanopb"
    consoleApplication: true

    Depends { name: "cpp" }
    cpp.cxxLanguageVersion: "c++11"
    cpp.minimumMacosVersion: "10.8"

    Depends { name: "protobuf.nanopb"; required: false }
    property bool hasProtobuf: {
        console.info("has protobuf: " + protobuf.nanopb.present);
        console.info("has modules: false");
        return protobuf.nanopb.present;
    }
    protobuf.nanopb.importPaths: product.sourceDirectory

    files: [
        "addressbook_nanopb.proto",
        "addressbook_nanopb.options",
        "main_nanopb.cpp",
    ]
}
