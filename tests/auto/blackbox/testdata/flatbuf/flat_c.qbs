CppApplication {
    Depends { name: "flatbuffers.c"; required: false }

    consoleApplication: true
    condition: {
        var result = qbs.targetPlatform === qbs.hostPlatform;
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
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
