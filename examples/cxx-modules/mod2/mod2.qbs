CppApplication {
    condition: {
        if (qbs.toolchainType === "msvc"
            // see https://gcc.gnu.org/pipermail/gcc-bugs/2023-November/842674.html
            // || qbs.toolchainType === "gcc"
            // || qbs.toolchainType === "mingw"
            || (qbs.toolchainType === "clang" && cpp.compilerVersionMajor >= 16))
            return true;
        console.info("Unsupported toolchainType " + qbs.toolchainType);
        return false;
    }
    consoleApplication: true
    files: [
        "mod2.cppm",
        "mod2io.cpp",
        "mod2order.cppm",
        "mod2price.cpp",
        "testmod2.cpp",
    ]
    cpp.cxxLanguageVersion: "c++20"
    cpp.forceUseCxxModules: true
    cpp.treatWarningsAsErrors: true
}
