import qbs

// just to make sure three-digit minimum versions work on macOS
// this only affects the value of __MAC_OS_X_VERSION_MIN_REQUIRED,
// not the actual LC_VERSION_MIN_MACOSX command which is limited to two
CppApplication {
    condition: qbs.targetOS.contains("macos")
    files: ["main.mm"]
    consoleApplication: true
    cpp.frameworks: "Foundation"
    cpp.minimumMacosVersion: "10.6.8"
}
