import qbs.Host

CppApplication {
    Depends { name: "capnproto.cpp"; required: false }
    condition: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch");
        if (!capnproto.cpp.present)
            console.info("capnproto is not present");
        return result && capnproto.cpp.present;
    }
    cpp.minimumMacosVersion: "10.8"
    files: [
        "bar.capnp",
        "capnproto_relative_import.cpp",
        "foo.capnp",
    ]
}
