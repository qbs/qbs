CppApplication {
    files: ["main.c"]

    // Minimum deployment targets that:
    // - will actually link (as of Xcode 8.1)
    // - exist for the given architecture(s)
    cpp.minimumMacosVersion: {
        if (qbs.architecture === "arm64")
            return "11.0";
        if (qbs.architecture === "x86_64h")
            return "10.12";
        return "10.6";
    }
    cpp.minimumIosVersion: ["armv7s", "arm64", "x86_64"].includes(qbs.architecture) ? "7.0" : "6.0"
    cpp.minimumTvosVersion: "9.0"
    cpp.minimumWatchosVersion: "2.0"

    cpp.driverFlags: ["-v"]
    cpp.linkerFlags: ["-v"]
}
