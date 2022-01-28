import qbs.Host

StaticLibrary {
    condition: {
        var result = qbs.targetPlatform === Host.platform();
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result && hasProtobuf;
    }

    Depends { name: "cpp" }
    cpp.cxxLanguageVersion: "c++11"
    cpp.minimumMacosVersion: "10.8"

    protobuf.cpp.importPaths: product.sourceDirectory

    Depends { name: "protobuf.cpp"; required: false }
    property bool hasProtobuf: {
        console.info("has protobuf: " + protobuf.cpp.present);
        return protobuf.cpp.present;
    }

    Group {
        fileTagsFilter: "protobuf.hpp"
        qbs.installDir: "include"
        qbs.installSourceBase: protobuf.cpp.outputDir
        qbs.install: true
    }

    files: [
        "hello.proto",
        "hello/world.proto",
    ]
}
