// myproject.qbs
CppApplication {
    condition: {
        if (qbs.toolchainType === "msvc"
            || ((qbs.toolchainType === "gcc" || qbs.toolchainType === "mingw")
                && cpp.compilerVersionMajor >= 11)
            || (qbs.toolchainType === "clang" && cpp.compilerVersionMajor >= 16)) {
            return true;
        }
        console.info("Unsupported toolchainType " + qbs.toolchainType);
        return false;
    }
    consoleApplication: true
    install: true
    files: ["hello.cppm", "main.cpp" ]
    cpp.cxxLanguageVersion: "c++20"
    cpp.forceUseCxxModules: true
}
