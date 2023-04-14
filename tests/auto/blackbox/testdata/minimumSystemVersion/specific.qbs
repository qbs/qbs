import qbs.Utilities

// a specific version of the operating systems is specified
// when the application is run its output should confirm
// that the given values took effect
import qbs.Host

CppApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform();
        if (!result)
            console.info("targetPlatform differs from hostPlatform");
        return result && qbs.targetOS.includes("windows") || qbs.targetOS.includes("macos");
    }
    files: [qbs.targetOS.includes("darwin") ? "main.mm" : "main.cpp"]
    consoleApplication: true

    Properties {
        condition: qbs.targetOS.includes("windows")
        cpp.minimumWindowsVersion: "6.2"
        cpp.defines: [
            "QBS_WINVER=0x602",
            "TOOLCHAIN_INSTALL_PATH=" + Utilities.cStringQuote(cpp.toolchainInstallPath)
        ]
    }

    Properties {
        condition: qbs.targetOS.includes("macos")
        cpp.frameworks: "Foundation"
        cpp.minimumMacosVersion: "10.7"
    }
}
