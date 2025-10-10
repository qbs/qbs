CppApplication {
    condition: {
        if (qbs.toolchainType === "msvc"
            || (qbs.toolchainType === "gcc" && cpp.compilerVersionMajor >= 11)
            || (qbs.toolchainType === "mingw" && cpp.compilerVersionMajor >= 13)
            || (qbs.toolchainType === "clang" && cpp.compilerVersionMajor >= 16))
            return true;
        console.info("Unsupported toolchainType " + qbs.toolchainType);
        return false;
    }
    consoleApplication: true
    files: [
        "mod0.cppm",
        "mod0main.cpp",
    ]
    cpp.cxxLanguageVersion: "c++20"
    cpp.forceUseCxxModules: true
    cpp.treatWarningsAsErrors: true
}
