Project {
    CppApplication {
        Depends { name: "capnproto.cpp"; required: false }
        name: "server"
        condition: capnproto.cpp.present && qbs.targetPlatform === qbs.hostPlatform
        consoleApplication: true
        capnproto.cpp.useRpc: true

        files: [
            "calculator.capnp",
            "calculator-server.cpp"
        ]
    }
    CppApplication {
        Depends { name: "capnproto.cpp"; required: false }
        name: "client"
        condition: capnproto.cpp.present && qbs.targetPlatform === qbs.hostPlatform
        consoleApplication: true
        capnproto.cpp.useRpc: true

        files: [
            "calculator.capnp",
            "calculator-client.cpp"
        ]
    }
}
