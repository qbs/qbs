CppApplication {
    condition: {
        var result = qbs.targetPlatform === qbs.hostPlatform;
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result;
    }
    consoleApplication: true
    cpp.defines: [
        'HOST_ARCHITECTURE="' + qbs.hostArchitecture + '"',
        'HOST_PLATFORM="' + qbs.hostPlatform + '"'
    ]
    files: "main.cpp"
}
