CppApplication {
    Depends { name: "flatbuf.cpp"; required: false }

    consoleApplication: true
    condition: {
        var result = qbs.targetPlatform === qbs.hostPlatform;
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result && hasFlatbuffers;
    }
    property bool hasFlatbuffers: {
        console.info("has flatbuffers: " + flatbuf.cpp.present);
        return flatbuf.cpp.present;
    }

    flatbuf.cpp.importPaths: "imports/"

    files: [
        "flat_absolute_import.cpp",
        "baz.fbs",
        "imports/imported_foo/imported_foo.fbs",
    ]
    qbsModuleProviders: "conan"
}
