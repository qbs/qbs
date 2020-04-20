CppApplication {
    property string sanitizer

    property bool supportsSanitizer: {
        if (qbs.toolchain.contains("clang-cl"))
            // only these are supported
            return sanitizer === "address" || sanitizer === "undefined";
        if (!qbs.toolchain.contains("gcc"))
            return false;
        if (qbs.toolchain.contains("mingw"))
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
    cpp.driverFlags: sanitizer ? ["-fsanitize=" + sanitizer] : []
    cpp.debugInformation: true
    files: "sanitizer.cpp"
}
