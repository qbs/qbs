import qbs.Host

CppApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform();
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        console.info("executable suffix: " + cpp.executableSuffix);
        return result;
    }
    name: "the app"
    consoleApplication: true

    cpp.cxxLanguageVersion: "c++11"
    cpp.separateDebugInformation: false
    Properties {
        condition: qbs.targetOS.includes("macos")
        bundle.embedInfoPlist: false
        cpp.minimumMacosVersion: "10.7"
    }

    files: "main.cpp"
    qbs.installPrefix: "/usr/local"
    install: true
}
