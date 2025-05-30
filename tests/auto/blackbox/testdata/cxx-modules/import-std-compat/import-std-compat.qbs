// Checks simple case with a single module.
CppApplication {
    name: "import-std-compat"
    condition: {
        if (qbs.toolchainType === "msvc"
            || ((qbs.toolchainType === "gcc" || qbs.toolchainType === "mingw")
                && cpp.compilerVersionMajor >= 15)
            || (qbs.toolchainType === "clang" && cpp.compilerVersionMajor >= 18)) {
            return true;
        }
        console.info("Unsupported toolchainType " + qbs.toolchainType);
        return false;
    }
    consoleApplication: true
    files: [
        "main.cpp"
    ]
    cpp.cxxLanguageVersion: "c++23"
    cpp.forceUseCxxModules: true
    cpp.forceUseImportStdCompat: true
    Properties {
        condition: qbs.toolchainType === "clang"
        cpp.cxxFlags: ["-Wno-reserved-module-identifier"]
        cpp.cxxStandardLibrary: "libc++"
    }
}