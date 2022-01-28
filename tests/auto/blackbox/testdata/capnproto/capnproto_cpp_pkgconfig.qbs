import qbs.Host

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
    cpp.minimumMacosVersion: "10.8"
    files: [
        "capnproto_cpp.cpp",
        "foo.capnp"
    ]
    qbsModuleProviders: "qbspkgconfig"
}
