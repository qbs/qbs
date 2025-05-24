import qbs.Host

CppApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch");
        return result;
    }
    consoleApplication: true
    cpp.defines: [
        'HOST_ARCHITECTURE="' + Host.architecture() + '"',
        'HOST_PLATFORM="' + Host.platform() + '"'
    ]
    files: "main.cpp"
}
