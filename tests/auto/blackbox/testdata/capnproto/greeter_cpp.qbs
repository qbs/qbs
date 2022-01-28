import qbs.Host

Project {
    CppApplication {
        Depends { name: "capnproto.cpp"; required: false }
        condition: {
            var result = qbs.targetPlatform === Host.platform();
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            if (!capnproto.cpp.present)
                console.info("capnproto is not present");
            return result && capnproto.cpp.present;
        }
        name: "server"
        consoleApplication: true
        cpp.minimumMacosVersion: "10.8"
        capnproto.cpp.useRpc: true
        files: [
            "greeter.capnp",
            "greeter-server.cpp"
        ]
    }
    CppApplication {
        Depends { name: "capnproto.cpp"; required: false }
        name: "client"
        consoleApplication: true
        capnproto.cpp.useRpc: true
        cpp.minimumMacosVersion: "10.8"
        files: [
            "greeter.capnp",
            "greeter-client.cpp"
        ]
    }
}
