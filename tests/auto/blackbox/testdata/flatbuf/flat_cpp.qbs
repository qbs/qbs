import qbs.Host

CppApplication {
    Depends { name: "flatbuf.cpp"; required: false }

    consoleApplication: true
    condition: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch");
        return result && hasFlatbuffers;
    }
    property bool hasFlatbuffers: {
        console.info("has flatbuffers: " + flatbuf.cpp.present);
        return flatbuf.cpp.present;
    }

    files: [
        "flat.cpp",
        "foo.fbs",
    ]
    qbsModuleProviders: "conan"
}
