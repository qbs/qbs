// Checks that module can be split into a declaration module and good old cpp implementation file
CppApplication {
    condition: {
        if (qbs.toolchainType === "msvc"
            || qbs.toolchainType === "gcc"
            || qbs.toolchainType === "mingw"
            || (qbs.toolchainType === "clang" && cpp.compilerVersionMajor >= 16))
            return true;
        console.info("Unsupported toolchainType " + qbs.toolchainType);
        return false;
    }
    consoleApplication: true
    files: [
        "hello.cppm",
        "hello.cpp",
        "main.cpp"
    ]
    cpp.cxxLanguageVersion: "c++20"
    cpp.forceUseCxxModules: true
    cpp.treatWarningsAsErrors: true
}
