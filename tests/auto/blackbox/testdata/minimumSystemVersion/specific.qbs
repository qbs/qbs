import qbs.Utilities

// a specific version of the operating systems is specified
// when the application is run its output should confirm
// that the given values took effect
CppApplication {
    condition: {
        var result = qbs.targetPlatform === qbs.hostPlatform;
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result && qbs.targetOS.contains("windows") || qbs.targetOS.contains("macos");
    }
    files: [qbs.targetOS.contains("darwin") ? "main.mm" : "main.cpp"]
    consoleApplication: true

    Properties {
        condition: qbs.targetOS.contains("windows")
        cpp.minimumWindowsVersion: "6.0"
        cpp.defines: [
            "QBS_WINVER=0x600",
            "TOOLCHAIN_INSTALL_PATH=" + Utilities.cStringQuote(cpp.toolchainInstallPath)
        ]
    }

    Properties {
        condition: qbs.targetOS.contains("macos")
        cpp.frameworks: "Foundation"
        cpp.minimumMacosVersion: "10.7"
    }
}
