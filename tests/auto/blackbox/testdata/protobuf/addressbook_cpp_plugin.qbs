import qbs.Host

CppApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch ("
                         + qbs.targetPlatform + "/" + qbs.architecture + " vs "
                         + Host.platform() + "/" + Host.architecture() + ")");
        return result && hasProtobuf;
    }
    name: "addressbook_cpp_plugin"
    consoleApplication: true

    Depends { name: "cpp" }
    cpp.minimumMacosVersion: "10.15"

    // Exercise protobuf.cpp.pluginName/pluginPath. Codegen still uses the built-in
    // C++ generator (--cpp_out); the plugin is registered but not invoked.
    Depends { name: "protobuf.cpp"; required: false }
    property bool hasProtobuf: {
        console.info("has protobuf: " + protobuf.cpp.present);
        console.info("has modules: " + protobuflib.present);
        return protobuf.cpp.present;
    }

    protobuf.cpp.pluginName: "protoc-gen-dummy"
    protobuf.cpp.pluginPath: product.sourceDirectory + "/dummy-protoc-plugin"

    files: [
        "addressbook.proto",
        "main.cpp",
    ]
}
