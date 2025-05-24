import qbs.Host
import qbs.Utilities

// non-existent versions of Windows should print a QBS warning
// (but will still compile and link since we avoid passing a
// bad value to the linker)
CppApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch");
        return result && qbs.targetOS.includes("windows");
    }
    files: ["main.cpp"]
    consoleApplication: true
    cpp.minimumWindowsVersion: "5.3"
    cpp.defines: [
        "QBS_WINVER=0x503",
        "TOOLCHAIN_INSTALL_PATH=" + Utilities.cStringQuote(cpp.toolchainInstallPath)
    ]
}
