import qbs.Host

CppApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform();
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result;
    }
    consoleApplication: true
    cpp.defines: [
        'HOST_ARCHITECTURE="' + Host.architecture() + '"',
        'HOST_PLATFORM="' + Host.platform() + '"'
    ]
    files: "main.cpp"
}
