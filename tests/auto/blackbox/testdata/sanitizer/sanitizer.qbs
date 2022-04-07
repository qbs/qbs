CppApplication {
    Depends { name: "Sanitizers.address" }
    Sanitizers.address.enabled: sanitizer === "address"
    property string sanitizer

    property bool supportsSanitizer: {
        if (qbs.toolchain.contains("mingw"))
            return false;
        if (sanitizer === "address")
            return Sanitizers.address._supported;
        if (qbs.toolchain.contains("clang-cl")) {
            if (cpp.toolchainInstallPath.contains("Microsoft Visual Studio")
                    && qbs.architecture === "x86_64") {
                // 32 bit sanitizer shipped with VS misses the x86_64 libraries
                return false;
            }
            // only these are supported
            return sanitizer === "address" || sanitizer === "undefined";
        }
        if (!qbs.toolchain.contains("gcc"))
            return false;
        if (qbs.targetOS.contains("ios")) {
            // thread sanitizer is not supported
            return sanitizer !== "thread";
        }
        return true;
    }

    condition: {
        if (!sanitizer)
            return true;
        if (!supportsSanitizer)
            console.info("Compiler does not support sanitizer");
        return supportsSanitizer;
    }
    qbs.buildVariant: "release"
    cpp.cxxLanguageVersion: "c++11"
    cpp.minimumMacosVersion: "10.8"
    consoleApplication: true
    cpp.runtimeLibrary: "static"
    cpp.driverFlags: sanitizer && sanitizer !== "address" ? ["-fsanitize=" + sanitizer] : []
    cpp.debugInformation: true
    files: "sanitizer.cpp"
}
