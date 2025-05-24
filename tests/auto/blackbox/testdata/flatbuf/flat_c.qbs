import qbs.Host
CppApplication {
    Depends { name: "flatbuffers.c"; required: false }

    consoleApplication: true
    condition: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch");
        return result && hasFlatbuffers;
    }
    property bool hasFlatbuffers: {
        console.info("has flatbuffers: " + flatbuffers.c.present);
        return flatbuffers.c.present;
    }

    files: [
        "flat.c",
        "foo.fbs",
    ]
    qbsModuleProviders: "conan"
}
