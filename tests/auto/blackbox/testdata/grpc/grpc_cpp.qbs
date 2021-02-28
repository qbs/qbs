import qbs.Host

CppApplication {
    name: "grpc_cpp"
    consoleApplication: true
    condition: {
        var result = qbs.targetPlatform === Host.platform();
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result && hasDependencies;
    }

    Depends { name: "cpp" }
    cpp.cxxLanguageVersion: "c++17"
    cpp.minimumMacosVersion: "10.15"
    cpp.warningLevel: "none"
    qbs.buildVariant: "release"

    Depends { name: "protobuf.cpp"; required: false }
    Depends { name: "grpc++"; id: grpcpp; required: false }
    protobuf.cpp.useGrpc: true

    property bool hasDependencies: {
        console.info("has grpc: " + protobuf.cpp.present);
        console.info("has modules: " + grpcpp.present);
        return protobuf.cpp.present && grpcpp.present;
    }

    files: "grpc.cpp"

    Group {
        files: "grpc.proto"
        fileTags: "protobuf.grpc"
    }
}
