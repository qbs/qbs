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
    capnproto.cpp.importPaths:  "."
    files: [
        "baz.capnp",
        "capnproto_absolute_import.cpp",
        "imports/foo.capnp",
    ]
    qbs.buildVariant: "release"
}
