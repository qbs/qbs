import qbs.Utilities

// non-existent versions of Windows should print a QBS warning
// (but will still compile and link since we avoid passing a
// bad value to the linker)
CppApplication {
    condition: qbs.targetOS.contains("windows")
    files: ["main.cpp"]
    consoleApplication: true
    cpp.minimumWindowsVersion: "5.3"
    cpp.defines: [
        "QBS_WINVER=0x503",
        "TOOLCHAIN_INSTALL_PATH=" + Utilities.cStringQuote(cpp.toolchainInstallPath)
    ]
}
