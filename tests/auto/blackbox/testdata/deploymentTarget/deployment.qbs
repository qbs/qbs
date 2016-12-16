import qbs

CppApplication {
    files: ["main.c"]

    // Minimum deployment targets that:
    // - will actually link (as of Xcode 8.1)
    // - exist for the given architecture(s)
    cpp.minimumMacosVersion: qbs.architecture === "x86_64h" ? "10.12" : "10.4"
    cpp.minimumIosVersion: ["armv7s", "arm64", "x86_64"].contains(qbs.architecture) ? "7.0" : "6.0"
    cpp.minimumTvosVersion: "9.0"
    cpp.minimumWatchosVersion: "2.0"

    cpp.driverFlags: ["-v"]
    cpp.linkerFlags: ["-v"]

    targetName: {
        if (cpp.compilerVersionMajor < 6 && qbs.architecture === "x86_64h")
            throw("x86_64h will be mis-detected as x86_64 with Apple Clang < 6.0");
        return name;
    }
}
