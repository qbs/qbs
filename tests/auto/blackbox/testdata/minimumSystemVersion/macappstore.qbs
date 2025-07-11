// just to make sure three-digit minimum versions work on macOS
// this only affects the value of __MAC_OS_X_VERSION_MIN_REQUIRED,
// not the actual LC_VERSION_MIN_MACOSX command which is limited to two
import qbs.Host

CppApplication {
    condition: {
        var result = qbs.targetPlatform === Host.platform() && qbs.architecture === Host.architecture();
        if (!result)
            console.info("target platform/arch differ from host platform/arch");
        return result && qbs.targetOS.includes("macos");
    }
    files: ["main.mm"]
    consoleApplication: true
    cpp.frameworks: "Foundation"
    cpp.minimumMacosVersion: "10.7.1"
}
