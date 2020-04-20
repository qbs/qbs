import qbs

CppApplication {
    name: "grpc_cpp"
    consoleApplication: true
    condition: {
        var result = qbs.targetPlatform === qbs.hostPlatform;
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result && hasDependencies;
    }

    Depends { name: "cpp" }
    cpp.cxxLanguageVersion: "c++11"
    cpp.minimumMacosVersion: "10.8"
    cpp.warningLevel: "none"

    Depends { name: "protobuf.cpp"; required: false }
    protobuf.cpp.useGrpc: true

    property bool hasDependencies: {
        console.info("has grpc: " + protobuf.cpp.present);
        return protobuf.cpp.present;
    }

    files: "grpc.cpp"

    Group {
        files: "grpc.proto"
        fileTags: "protobuf.grpc"
    }
}
